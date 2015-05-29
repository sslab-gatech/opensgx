/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2009 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "red_common.h"
#include "mjpeg_encoder.h"
#include <jerror.h>
#include <jpeglib.h>

#define MJPEG_MAX_FPS 25
#define MJPEG_MIN_FPS 1

#define MJPEG_QUALITY_SAMPLE_NUM 7
static const int mjpeg_quality_samples[MJPEG_QUALITY_SAMPLE_NUM] = {20, 30, 40, 50, 60, 70, 80};

#define MJPEG_LEGACY_STATIC_QUALITY_ID 5 // jpeg quality 70

#define MJPEG_IMPROVE_QUALITY_FPS_STRICT_TH 10
#define MJPEG_IMPROVE_QUALITY_FPS_PERMISSIVE_TH 5

#define MJPEG_AVERAGE_SIZE_WINDOW 3

#define MJPEG_BIT_RATE_EVAL_MIN_NUM_FRAMES 3
#define MJPEG_LOW_FPS_RATE_TH 3

#define MJPEG_SERVER_STATUS_EVAL_FPS_INTERVAL 1
#define MJPEG_SERVER_STATUS_DOWNGRADE_DROP_FACTOR_TH 0.1

/*
 * acting on positive client reports only if enough frame mm time
 * has passed since the last bit rate change and the report.
 * time
 */
#define MJPEG_CLIENT_POSITIVE_REPORT_TIMEOUT 2000
#define MJPEG_CLIENT_POSITIVE_REPORT_STRICT_TIMEOUT 3000

#define MJPEG_ADJUST_FPS_TIMEOUT 500

/*
 * avoid interrupting the playback when there are temporary
 * incidents of instability (with respect to server and client drops)
 */
#define MJPEG_MAX_CLIENT_PLAYBACK_DELAY 5000 // 5 sec

/*
 * The stream starts after lossless frames were sent to the client,
 * and without rate control (except for pipe congestion). Thus, on the beginning
 * of the stream, we might observe frame drops on the client and server side which
 * are not necessarily related to mis-estimation of the bit rate, and we would
 * like to wait till the stream stabilizes.
 */
#define MJPEG_WARMUP_TIME 3000L // 3 sec

enum {
    MJPEG_QUALITY_EVAL_TYPE_SET,
    MJPEG_QUALITY_EVAL_TYPE_UPGRADE,
    MJPEG_QUALITY_EVAL_TYPE_DOWNGRADE,
};

enum {
    MJPEG_QUALITY_EVAL_REASON_SIZE_CHANGE,
    MJPEG_QUALITY_EVAL_REASON_RATE_CHANGE,
};

typedef struct MJpegEncoderQualityEval {
    int type;
    int reason;

    uint64_t encoded_size_by_quality[MJPEG_QUALITY_SAMPLE_NUM];
    /* lower limit for the current evaluation round */
    int min_quality_id;
    int min_quality_fps; // min fps for the given quality
    /* upper limit for the current evaluation round */
    int max_quality_id;
    int max_quality_fps; // max fps for the given quality
    /* tracking the best sampled fps so far */
    int max_sampled_fps;
    int max_sampled_fps_quality_id;
} MJpegEncoderQualityEval;

typedef struct MJpegEncoderClientState {
    int max_video_latency;
    uint32_t max_audio_latency;
} MJpegEncoderClientState;

typedef struct MJpegEncoderServerState {
    uint32_t num_frames_encoded;
    uint32_t num_frames_dropped;
} MJpegEncoderServerState;

typedef struct MJpegEncoderBitRateInfo {
    uint64_t change_start_time;
    uint64_t last_frame_time;
    uint32_t change_start_mm_time;
    int was_upgraded;

    /* gathering data about the frames that
     * were encoded since the last bit rate change*/
    uint32_t num_enc_frames;
    uint64_t sum_enc_size;
} MJpegEncoderBitRateInfo;

/*
 * Adjusting the stream jpeg quality and frame rate (fps):
 * When during_quality_eval=TRUE, we compress different frames with different
 * jpeg quality. By considering (1) the resulting compression ratio, and (2) the available
 * bit rate, we evaluate the max frame frequency for the stream with the given quality,
 * and we choose the highest quality that will allow a reasonable frame rate.
 * during_quality_eval is set for new streams and can also be set any time we want
 * to re-evaluate the stream parameters (e.g., when the bit rate and/or
 * compressed frame size significantly change).
 */
typedef struct MJpegEncoderRateControl {
    int during_quality_eval;
    MJpegEncoderQualityEval quality_eval_data;
    MJpegEncoderBitRateInfo bit_rate_info;
    MJpegEncoderClientState client_state;
    MJpegEncoderServerState server_state;

    uint64_t byte_rate;
    int quality_id;
    uint32_t fps;
    double adjusted_fps;
    uint64_t adjusted_fps_start_time;
    uint64_t adjusted_fps_num_frames;

    /* the encoded frame size which the quality and the fps evaluation was based upon */
    uint64_t base_enc_size;

    uint64_t last_enc_size;

    uint64_t sum_recent_enc_size;
    uint32_t num_recent_enc_frames;

    uint64_t warmup_start_time;
} MJpegEncoderRateControl;

struct MJpegEncoder {
    uint8_t *row;
    uint32_t row_size;
    int first_frame;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    unsigned int bytes_per_pixel; /* bytes per pixel of the input buffer */
    void (*pixel_converter)(uint8_t *src, uint8_t *dest);

    int rate_control_is_active;
    MJpegEncoderRateControl rate_control;
    MJpegEncoderRateControlCbs cbs;
    void *cbs_opaque;

    /* stats */
    uint64_t starting_bit_rate;
    uint64_t avg_quality;
    uint32_t num_frames;
};

static inline void mjpeg_encoder_reset_quality(MJpegEncoder *encoder,
                                               int quality_id,
                                               uint32_t fps,
                                               uint64_t frame_enc_size);
static uint32_t get_max_fps(uint64_t frame_size, uint64_t bytes_per_sec);
static void mjpeg_encoder_process_server_drops(MJpegEncoder *encoder);
static uint32_t get_min_required_playback_delay(uint64_t frame_enc_size,
                                                uint64_t byte_rate,
                                                uint32_t latency);

MJpegEncoder *mjpeg_encoder_new(int bit_rate_control, uint64_t starting_bit_rate,
                                MJpegEncoderRateControlCbs *cbs, void *opaque)
{
    MJpegEncoder *enc;

    spice_assert(!bit_rate_control || (cbs && cbs->get_roundtrip_ms && cbs->get_source_fps));

    enc = spice_new0(MJpegEncoder, 1);

    enc->first_frame = TRUE;
    enc->rate_control_is_active = bit_rate_control;
    enc->rate_control.byte_rate = starting_bit_rate / 8;
    enc->starting_bit_rate = starting_bit_rate;

    if (bit_rate_control) {
        struct timespec time;

        clock_gettime(CLOCK_MONOTONIC, &time);
        enc->cbs = *cbs;
        enc->cbs_opaque = opaque;
        mjpeg_encoder_reset_quality(enc, MJPEG_QUALITY_SAMPLE_NUM / 2, 5, 0);
        enc->rate_control.during_quality_eval = TRUE;
        enc->rate_control.quality_eval_data.type = MJPEG_QUALITY_EVAL_TYPE_SET;
        enc->rate_control.quality_eval_data.reason = MJPEG_QUALITY_EVAL_REASON_RATE_CHANGE;
        enc->rate_control.warmup_start_time = ((uint64_t) time.tv_sec) * 1000000000 + time.tv_nsec;
    } else {
        mjpeg_encoder_reset_quality(enc, MJPEG_LEGACY_STATIC_QUALITY_ID, MJPEG_MAX_FPS, 0);
    }

    enc->cinfo.err = jpeg_std_error(&enc->jerr);
    jpeg_create_compress(&enc->cinfo);

    return enc;
}

void mjpeg_encoder_destroy(MJpegEncoder *encoder)
{
    jpeg_destroy_compress(&encoder->cinfo);
    free(encoder->row);
    free(encoder);
}

uint8_t mjpeg_encoder_get_bytes_per_pixel(MJpegEncoder *encoder)
{
    return encoder->bytes_per_pixel;
}

#ifndef JCS_EXTENSIONS
/* Pixel conversion routines */
static void pixel_rgb24bpp_to_24(uint8_t *src, uint8_t *dest)
{
    /* libjpegs stores rgb, spice/win32 stores bgr */
    *dest++ = src[2]; /* red */
    *dest++ = src[1]; /* green */
    *dest++ = src[0]; /* blue */
}

static void pixel_rgb32bpp_to_24(uint8_t *src, uint8_t *dest)
{
    uint32_t pixel = *(uint32_t *)src;
    *dest++ = (pixel >> 16) & 0xff;
    *dest++ = (pixel >>  8) & 0xff;
    *dest++ = (pixel >>  0) & 0xff;
}
#endif

static void pixel_rgb16bpp_to_24(uint8_t *src, uint8_t *dest)
{
    uint16_t pixel = *(uint16_t *)src;
    *dest++ = ((pixel >> 7) & 0xf8) | ((pixel >> 12) & 0x7);
    *dest++ = ((pixel >> 2) & 0xf8) | ((pixel >> 7) & 0x7);
    *dest++ = ((pixel << 3) & 0xf8) | ((pixel >> 2) & 0x7);
}


/* code from libjpeg 8 to handle compression to a memory buffer
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * Modified 2009 by Guido Vollbeding.
 * This file is part of the Independent JPEG Group's software.
 */
typedef struct {
  struct jpeg_destination_mgr pub; /* public fields */

  unsigned char ** outbuffer;	/* target buffer */
  size_t * outsize;
  uint8_t * buffer;		/* start of buffer */
  size_t bufsize;
} mem_destination_mgr;

static void init_mem_destination(j_compress_ptr cinfo)
{
}

static boolean empty_mem_output_buffer(j_compress_ptr cinfo)
{
  size_t nextsize;
  uint8_t * nextbuffer;
  mem_destination_mgr *dest = (mem_destination_mgr *) cinfo->dest;

  /* Try to allocate new buffer with double size */
  nextsize = dest->bufsize * 2;
  nextbuffer = malloc(nextsize);

  if (nextbuffer == NULL)
    ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 10);

  memcpy(nextbuffer, dest->buffer, dest->bufsize);

  free(dest->buffer);

  dest->pub.next_output_byte = nextbuffer + dest->bufsize;
  dest->pub.free_in_buffer = dest->bufsize;

  dest->buffer = nextbuffer;
  dest->bufsize = nextsize;

  return TRUE;
}

static void term_mem_destination(j_compress_ptr cinfo)
{
  mem_destination_mgr *dest = (mem_destination_mgr *) cinfo->dest;

  *dest->outbuffer = dest->buffer;
  *dest->outsize = dest->bufsize;
}

/*
 * Prepare for output to a memory buffer.
 * The caller may supply an own initial buffer with appropriate size.
 * Otherwise, or when the actual data output exceeds the given size,
 * the library adapts the buffer size as necessary.
 * The standard library functions malloc/free are used for allocating
 * larger memory, so the buffer is available to the application after
 * finishing compression, and then the application is responsible for
 * freeing the requested memory.
 */

static void
spice_jpeg_mem_dest(j_compress_ptr cinfo,
                    unsigned char ** outbuffer, size_t * outsize)
{
  mem_destination_mgr *dest;
#define OUTPUT_BUF_SIZE  4096	/* choose an efficiently fwrite'able size */

  if (outbuffer == NULL || outsize == NULL)	/* sanity check */
    ERREXIT(cinfo, JERR_BUFFER_SIZE);

  /* The destination object is made permanent so that multiple JPEG images
   * can be written to the same buffer without re-executing jpeg_mem_dest.
   */
  if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
    cinfo->dest = spice_malloc(sizeof(mem_destination_mgr));
  }

  dest = (mem_destination_mgr *) cinfo->dest;
  dest->pub.init_destination = init_mem_destination;
  dest->pub.empty_output_buffer = empty_mem_output_buffer;
  dest->pub.term_destination = term_mem_destination;
  dest->outbuffer = outbuffer;
  dest->outsize = outsize;
  if (*outbuffer == NULL || *outsize == 0) {
    /* Allocate initial buffer */
    *outbuffer = malloc(OUTPUT_BUF_SIZE);
    if (*outbuffer == NULL)
      ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 10);
    *outsize = OUTPUT_BUF_SIZE;
  }

  dest->pub.next_output_byte = dest->buffer = *outbuffer;
  dest->pub.free_in_buffer = dest->bufsize = *outsize;
}
/* end of code from libjpeg */

static inline uint32_t mjpeg_encoder_get_latency(MJpegEncoder *encoder)
{
    return encoder->cbs.get_roundtrip_ms ?
        encoder->cbs.get_roundtrip_ms(encoder->cbs_opaque) / 2 : 0;
}

static uint32_t get_max_fps(uint64_t frame_size, uint64_t bytes_per_sec)
{
    double fps;
    double send_time_ms;

    if (!bytes_per_sec) {
        return 0;
    }
    send_time_ms = frame_size * 1000.0 / bytes_per_sec;
    fps = send_time_ms ? 1000 / send_time_ms : MJPEG_MAX_FPS;
    return fps;
}

static inline void mjpeg_encoder_reset_quality(MJpegEncoder *encoder,
                                               int quality_id,
                                               uint32_t fps,
                                               uint64_t frame_enc_size)
{
    MJpegEncoderRateControl *rate_control = &encoder->rate_control;
    double fps_ratio;

    rate_control->during_quality_eval = FALSE;

    if (rate_control->quality_id != quality_id) {
        rate_control->last_enc_size = 0;
    }

    if (rate_control->quality_eval_data.reason == MJPEG_QUALITY_EVAL_REASON_RATE_CHANGE) {
        memset(&rate_control->server_state, 0, sizeof(MJpegEncoderServerState));
    }
    rate_control->quality_id = quality_id;
    memset(&rate_control->quality_eval_data, 0, sizeof(MJpegEncoderQualityEval));
    rate_control->quality_eval_data.max_quality_id = MJPEG_QUALITY_SAMPLE_NUM - 1;
    rate_control->quality_eval_data.max_quality_fps = MJPEG_MAX_FPS;

    if (rate_control->adjusted_fps) {
        fps_ratio = rate_control->adjusted_fps / rate_control->fps;
    } else {
        fps_ratio = 1.5;
    }
    rate_control->fps = MAX(MJPEG_MIN_FPS, fps);
    rate_control->fps = MIN(MJPEG_MAX_FPS, rate_control->fps);
    rate_control->adjusted_fps = rate_control->fps*fps_ratio;
    spice_debug("adjusted-fps-ratio=%.2f adjusted-fps=%.2f", fps_ratio, rate_control->adjusted_fps);
    rate_control->adjusted_fps_start_time = 0;
    rate_control->adjusted_fps_num_frames = 0;
    rate_control->base_enc_size = frame_enc_size;

    rate_control->sum_recent_enc_size = 0;
    rate_control->num_recent_enc_frames = 0;
}

#define QUALITY_WAS_EVALUATED(encoder, quality) \
    ((encoder)->rate_control.quality_eval_data.encoded_size_by_quality[(quality)] != 0)

/*
 * Adjust the stream's jpeg quality and frame rate.
 * We evaluate the compression ratio of different jpeg qualities;
 * We compress successive frames with different qualities,
 * and then we estimate the stream frame rate according to the currently
 * evaluated jpeg quality and available bit rate.
 *
 * During quality evaluation, mjpeg_encoder_eval_quality is called before a new
 * frame is encoded. mjpeg_encoder_eval_quality examines the encoding size of
 * the previously encoded frame, and determines whether to continue evaluation
 * (and chnages the quality for the frame that is going to be encoded),
 * or stop evaluation (and sets the quality and frame rate for the stream).
 * When qualities are scanned, we assume monotonicity of compression ratio
 * as a function of jpeg quality. When we reach a quality with too small, or
 * big enough compression ratio, we stop the evaluation and set the stream parameters.
*/
static inline void mjpeg_encoder_eval_quality(MJpegEncoder *encoder)
{
    MJpegEncoderRateControl *rate_control;
    MJpegEncoderQualityEval *quality_eval;
    uint32_t fps, src_fps;
    uint64_t enc_size;
    uint32_t final_quality_id;
    uint32_t final_fps;
    uint64_t final_quality_enc_size;

    rate_control = &encoder->rate_control;
    quality_eval = &rate_control->quality_eval_data;

    spice_assert(rate_control->during_quality_eval);

    /* retrieving the encoded size of the last encoded frame */
    enc_size = quality_eval->encoded_size_by_quality[rate_control->quality_id];
    if (enc_size == 0) {
        spice_debug("size info missing");
        return;
    }

    src_fps = encoder->cbs.get_source_fps(encoder->cbs_opaque);

    fps = get_max_fps(enc_size, rate_control->byte_rate);
    spice_debug("mjpeg %p: jpeg %d: %.2f (KB) fps %d src-fps %u",
                encoder,
                mjpeg_quality_samples[rate_control->quality_id],
                enc_size / 1024.0,
                fps,
                src_fps);

    if (fps > quality_eval->max_sampled_fps ||
        ((fps == quality_eval->max_sampled_fps || fps >= src_fps) &&
         rate_control->quality_id > quality_eval->max_sampled_fps_quality_id)) {
        quality_eval->max_sampled_fps = fps;
        quality_eval->max_sampled_fps_quality_id = rate_control->quality_id;
    }

    /*
     * Choosing whether to evaluate another quality, or to complete evaluation
     * and set the stream parameters according to one of the qualities that
     * were already sampled.
     */

    if (rate_control->quality_id > MJPEG_QUALITY_SAMPLE_NUM / 2 &&
        fps < MJPEG_IMPROVE_QUALITY_FPS_STRICT_TH &&
        fps < src_fps) {
        /*
         * When the jpeg quality is bigger than the median quality, prefer a reasonable
         * frame rate over improving the quality
         */
        spice_debug("fps < %d && (fps < src_fps), quality %d",
                MJPEG_IMPROVE_QUALITY_FPS_STRICT_TH,
                mjpeg_quality_samples[rate_control->quality_id]);
        if (QUALITY_WAS_EVALUATED(encoder, rate_control->quality_id - 1)) {
            /* the next worse quality was already evaluated and it passed the frame
             * rate thresholds (we know that, because we continued evaluating a better
             * quality) */
            rate_control->quality_id--;
            goto complete_sample;
        } else {
            /* evaluate the next worse quality */
            rate_control->quality_id--;
        }
    } else if ((fps > MJPEG_IMPROVE_QUALITY_FPS_PERMISSIVE_TH &&
                fps >= 0.66 * quality_eval->min_quality_fps) || fps >= src_fps) {
        /* When the jpeg quality is worse than the median one (see first condition), we allow a less
           strict threshold for fps, in order to improve the jpeg quality */
        if (rate_control->quality_id + 1 == MJPEG_QUALITY_SAMPLE_NUM ||
            rate_control->quality_id >= quality_eval->max_quality_id ||
            QUALITY_WAS_EVALUATED(encoder, rate_control->quality_id + 1)) {
            /* best quality has been reached, or the next (better) quality was
             * already evaluated and didn't pass the fps thresholds */
            goto complete_sample;
        } else {
            if (rate_control->quality_id == MJPEG_QUALITY_SAMPLE_NUM / 2 &&
                fps < MJPEG_IMPROVE_QUALITY_FPS_STRICT_TH &&
                fps < src_fps) {
                goto complete_sample;
            }
            /* evaluate the next quality as well*/
            rate_control->quality_id++;
        }
    } else { // very small frame rate, try to improve by downgrading the quality
        if (rate_control->quality_id == 0 ||
            rate_control->quality_id <= quality_eval->min_quality_id) {
            goto complete_sample;
        } else if (QUALITY_WAS_EVALUATED(encoder, rate_control->quality_id - 1)) {
            rate_control->quality_id--;
            goto complete_sample;
        } else {
            /* evaluate the next worse quality */
            rate_control->quality_id--;
        }
    }
    return;

complete_sample:
    if (quality_eval->max_sampled_fps != 0) {
        /* covering a case were monotonicity was violated and we sampled
           a better jepg quality, with better frame rate. */
        final_quality_id = MAX(rate_control->quality_id,
                               quality_eval->max_sampled_fps_quality_id);
    } else {
        final_quality_id = rate_control->quality_id;
    }
    final_quality_enc_size = quality_eval->encoded_size_by_quality[final_quality_id];
    final_fps = get_max_fps(final_quality_enc_size,
                            rate_control->byte_rate);

    if (final_quality_id == quality_eval->min_quality_id) {
        final_fps = MAX(final_fps, quality_eval->min_quality_fps);
    }
    if (final_quality_id == quality_eval->max_quality_id) {
        final_fps = MIN(final_fps, quality_eval->max_quality_fps);
    }
    mjpeg_encoder_reset_quality(encoder, final_quality_id, final_fps, final_quality_enc_size);
    rate_control->sum_recent_enc_size = final_quality_enc_size;
    rate_control->num_recent_enc_frames = 1;

    spice_debug("MJpeg quality sample end %p: quality %d fps %d",
                encoder, mjpeg_quality_samples[rate_control->quality_id], rate_control->fps);
    if (encoder->cbs.update_client_playback_delay) {
        uint32_t latency = mjpeg_encoder_get_latency(encoder);
        uint32_t min_delay = get_min_required_playback_delay(final_quality_enc_size,
                                                             rate_control->byte_rate,
                                                             latency);

        encoder->cbs.update_client_playback_delay(encoder->cbs_opaque, min_delay);
    }
}

static void mjpeg_encoder_quality_eval_set_upgrade(MJpegEncoder *encoder,
                                                   int reason,
                                                   uint32_t min_quality_id,
                                                   uint32_t min_quality_fps)
{
    MJpegEncoderQualityEval *quality_eval = &encoder->rate_control.quality_eval_data;

    encoder->rate_control.during_quality_eval = TRUE;
    quality_eval->type = MJPEG_QUALITY_EVAL_TYPE_UPGRADE;
    quality_eval->reason = reason;
    quality_eval->min_quality_id = min_quality_id;
    quality_eval->min_quality_fps = min_quality_fps;
}

static void mjpeg_encoder_quality_eval_set_downgrade(MJpegEncoder *encoder,
                                                     int reason,
                                                     uint32_t max_quality_id,
                                                     uint32_t max_quality_fps)
{
    MJpegEncoderQualityEval *quality_eval = &encoder->rate_control.quality_eval_data;

    encoder->rate_control.during_quality_eval = TRUE;
    quality_eval->type = MJPEG_QUALITY_EVAL_TYPE_DOWNGRADE;
    quality_eval->reason = reason;
    quality_eval->max_quality_id = max_quality_id;
    quality_eval->max_quality_fps = max_quality_fps;
}

static void mjpeg_encoder_adjust_params_to_bit_rate(MJpegEncoder *encoder)
{
    MJpegEncoderRateControl *rate_control;
    MJpegEncoderQualityEval *quality_eval;
    uint64_t new_avg_enc_size = 0;
    uint32_t new_fps;
    uint32_t latency = 0;
    uint32_t src_fps;

    if (!encoder->rate_control_is_active) {
        return;
    }

    rate_control = &encoder->rate_control;
    quality_eval = &rate_control->quality_eval_data;

    if (!rate_control->last_enc_size) {
        spice_debug("missing sample size");
        return;
    }

    if (rate_control->during_quality_eval) {
        quality_eval->encoded_size_by_quality[rate_control->quality_id] = rate_control->last_enc_size;
        mjpeg_encoder_eval_quality(encoder);
        return;
    }

    spice_assert(rate_control->num_recent_enc_frames);

    if (rate_control->num_recent_enc_frames < MJPEG_AVERAGE_SIZE_WINDOW &&
        rate_control->num_recent_enc_frames < rate_control->fps) {
        goto end;
    }

    latency = mjpeg_encoder_get_latency(encoder);
    new_avg_enc_size = rate_control->sum_recent_enc_size /
                       rate_control->num_recent_enc_frames;
    new_fps = get_max_fps(new_avg_enc_size, rate_control->byte_rate);

    spice_debug("cur-fps=%u new-fps=%u (new/old=%.2f) |"
                "bit-rate=%.2f (Mbps) latency=%u (ms) quality=%d |"
                " new-size-avg %lu , base-size %lu, (new/old=%.2f) ",
                rate_control->fps, new_fps, ((double)new_fps)/rate_control->fps,
                ((double)rate_control->byte_rate*8)/1024/1024,
                latency,
                mjpeg_quality_samples[rate_control->quality_id],
                new_avg_enc_size, rate_control->base_enc_size,
                rate_control->base_enc_size ?
                    ((double)new_avg_enc_size) / rate_control->base_enc_size :
                    1);

     src_fps = encoder->cbs.get_source_fps(encoder->cbs_opaque);

    /*
     * The ratio between the new_fps and the current fps reflects the changes
     * in latency and frame size. When the change passes a threshold,
     * we re-evaluate the quality and frame rate.
     */
    if (new_fps > rate_control->fps &&
        (rate_control->fps < src_fps || rate_control->quality_id < MJPEG_QUALITY_SAMPLE_NUM - 1)) {
        spice_debug("mjpeg %p FPS CHANGE >> :  re-evaluating params", encoder);
        mjpeg_encoder_quality_eval_set_upgrade(encoder, MJPEG_QUALITY_EVAL_REASON_SIZE_CHANGE,
                                               rate_control->quality_id, /* fps has improved -->
                                                                            don't allow stream quality
                                                                            to deteriorate */
                                               rate_control->fps);
    } else if (new_fps < rate_control->fps && new_fps < src_fps) {
        spice_debug("mjpeg %p FPS CHANGE << : re-evaluating params", encoder);
        mjpeg_encoder_quality_eval_set_downgrade(encoder, MJPEG_QUALITY_EVAL_REASON_SIZE_CHANGE,
                                                 rate_control->quality_id,
                                                 rate_control->fps);
    }
end:
    if (rate_control->during_quality_eval) {
        quality_eval->encoded_size_by_quality[rate_control->quality_id] = new_avg_enc_size;
        mjpeg_encoder_eval_quality(encoder);
    } else {
        mjpeg_encoder_process_server_drops(encoder);
    }
}

/*
 * The actual frames distribution does not necessarily fit the condition "at least
 * one frame every (1000/rate_contorl->fps) milliseconds".
 * For keeping the average fps close to the defined fps, we periodically
 * measure the current average fps, and modify rate_control->adjusted_fps accordingly.
 * Then, we use (1000/rate_control->adjusted_fps) as the interval between frames.
 */
static void mjpeg_encoder_adjust_fps(MJpegEncoder *encoder, uint64_t now)
{
    MJpegEncoderRateControl *rate_control = &encoder->rate_control;
    uint64_t adjusted_fps_time_passed;

    if (!encoder->rate_control_is_active) {
        return;
    }
    adjusted_fps_time_passed = (now - rate_control->adjusted_fps_start_time) / 1000 / 1000;

    if (!rate_control->during_quality_eval &&
        adjusted_fps_time_passed > MJPEG_ADJUST_FPS_TIMEOUT &&
        adjusted_fps_time_passed > 1000 / rate_control->adjusted_fps) {
        double avg_fps;
        double fps_ratio;

        avg_fps = ((double)rate_control->adjusted_fps_num_frames*1000) /
                  adjusted_fps_time_passed;
        spice_debug("#frames-adjust=%lu #adjust-time=%lu avg-fps=%.2f",
                    rate_control->adjusted_fps_num_frames, adjusted_fps_time_passed, avg_fps);
        spice_debug("defined=%u old-adjusted=%.2f", rate_control->fps, rate_control->adjusted_fps);
        fps_ratio = avg_fps / rate_control->fps;
        if (avg_fps + 0.5 < rate_control->fps &&
            encoder->cbs.get_source_fps(encoder->cbs_opaque) > avg_fps) {
            double new_adjusted_fps = avg_fps ?
                                               (rate_control->adjusted_fps/fps_ratio) :
                                               rate_control->adjusted_fps * 2;

            rate_control->adjusted_fps = MIN(rate_control->fps*2, new_adjusted_fps);
            spice_debug("new-adjusted-fps=%.2f", rate_control->adjusted_fps);
        } else if (rate_control->fps + 0.5 < avg_fps) {
            double new_adjusted_fps = rate_control->adjusted_fps / fps_ratio;

            rate_control->adjusted_fps = MAX(rate_control->fps, new_adjusted_fps);
            spice_debug("new-adjusted-fps=%.2f", rate_control->adjusted_fps);
        }
        rate_control->adjusted_fps_start_time = now;
        rate_control->adjusted_fps_num_frames = 0;
    }
}

int mjpeg_encoder_start_frame(MJpegEncoder *encoder, SpiceBitmapFmt format,
                              int width, int height,
                              uint8_t **dest, size_t *dest_len,
                              uint32_t frame_mm_time)
{
    uint32_t quality;

    if (encoder->rate_control_is_active) {
        MJpegEncoderRateControl *rate_control = &encoder->rate_control;
        struct timespec time;
        uint64_t now;
        uint64_t interval;

        clock_gettime(CLOCK_MONOTONIC, &time);
        now = ((uint64_t) time.tv_sec) * 1000000000 + time.tv_nsec;

        if (!rate_control->adjusted_fps_start_time) {
            rate_control->adjusted_fps_start_time = now;
        }
        mjpeg_encoder_adjust_fps(encoder, now);
        interval = (now - rate_control->bit_rate_info.last_frame_time);

        if (interval < (1000*1000*1000) / rate_control->adjusted_fps) {
            return MJPEG_ENCODER_FRAME_DROP;
        }

        mjpeg_encoder_adjust_params_to_bit_rate(encoder);

        if (!rate_control->during_quality_eval ||
            rate_control->quality_eval_data.reason == MJPEG_QUALITY_EVAL_REASON_SIZE_CHANGE) {
            MJpegEncoderBitRateInfo *bit_rate_info;

            bit_rate_info = &encoder->rate_control.bit_rate_info;

            if (!bit_rate_info->change_start_time) {
                bit_rate_info->change_start_time = now;
                bit_rate_info->change_start_mm_time = frame_mm_time;
            }
            bit_rate_info->last_frame_time = now;
        }
    }

    encoder->cinfo.in_color_space   = JCS_RGB;
    encoder->cinfo.input_components = 3;
    encoder->pixel_converter = NULL;

    switch (format) {
    case SPICE_BITMAP_FMT_32BIT:
    case SPICE_BITMAP_FMT_RGBA:
        encoder->bytes_per_pixel = 4;
#ifdef JCS_EXTENSIONS
        encoder->cinfo.in_color_space   = JCS_EXT_BGRX;
        encoder->cinfo.input_components = 4;
#else
        encoder->pixel_converter = pixel_rgb32bpp_to_24;
#endif
        break;
    case SPICE_BITMAP_FMT_16BIT:
        encoder->bytes_per_pixel = 2;
        encoder->pixel_converter = pixel_rgb16bpp_to_24;
        break;
    case SPICE_BITMAP_FMT_24BIT:
        encoder->bytes_per_pixel = 3;
#ifdef JCS_EXTENSIONS
        encoder->cinfo.in_color_space = JCS_EXT_BGR;
#else
        encoder->pixel_converter = pixel_rgb24bpp_to_24;
#endif
        break;
    default:
        spice_warning("unsupported format %d", format);
        return MJPEG_ENCODER_FRAME_UNSUPPORTED;
    }

    if (encoder->pixel_converter != NULL) {
        unsigned int stride = width * 3;
        /* check for integer overflow */
        if (stride < width) {
            return MJPEG_ENCODER_FRAME_UNSUPPORTED;
        }
        if (encoder->row_size < stride) {
            encoder->row = spice_realloc(encoder->row, stride);
            encoder->row_size = stride;
        }
    }

    spice_jpeg_mem_dest(&encoder->cinfo, dest, dest_len);

    encoder->cinfo.image_width      = width;
    encoder->cinfo.image_height     = height;
    jpeg_set_defaults(&encoder->cinfo);
    encoder->cinfo.dct_method       = JDCT_IFAST;
    quality = mjpeg_quality_samples[encoder->rate_control.quality_id];
    jpeg_set_quality(&encoder->cinfo, quality, TRUE);
    jpeg_start_compress(&encoder->cinfo, encoder->first_frame);

    encoder->num_frames++;
    encoder->avg_quality += quality;
    return MJPEG_ENCODER_FRAME_ENCODE_START;
}

int mjpeg_encoder_encode_scanline(MJpegEncoder *encoder, uint8_t *src_pixels,
                                  size_t image_width)
{
    unsigned int scanlines_written;
    uint8_t *row;

    row = encoder->row;
    if (encoder->pixel_converter) {
        unsigned int x;
        for (x = 0; x < image_width; x++) {
            encoder->pixel_converter(src_pixels, row);
            row += 3;
            src_pixels += encoder->bytes_per_pixel;
        }
        scanlines_written = jpeg_write_scanlines(&encoder->cinfo, &encoder->row, 1);
    } else {
        scanlines_written = jpeg_write_scanlines(&encoder->cinfo, &src_pixels, 1);
    }
    if (scanlines_written == 0) { /* Not enough space */
        jpeg_abort_compress(&encoder->cinfo);
        encoder->rate_control.last_enc_size = 0;
        return 0;
    }

    return scanlines_written;
}

size_t mjpeg_encoder_end_frame(MJpegEncoder *encoder)
{
    mem_destination_mgr *dest = (mem_destination_mgr *) encoder->cinfo.dest;
    MJpegEncoderRateControl *rate_control = &encoder->rate_control;

    jpeg_finish_compress(&encoder->cinfo);

    encoder->first_frame = FALSE;
    rate_control->last_enc_size = dest->pub.next_output_byte - dest->buffer;
    rate_control->server_state.num_frames_encoded++;

    if (!rate_control->during_quality_eval ||
        rate_control->quality_eval_data.reason == MJPEG_QUALITY_EVAL_REASON_SIZE_CHANGE) {

        if (!rate_control->during_quality_eval) {
            if (rate_control->num_recent_enc_frames >= MJPEG_AVERAGE_SIZE_WINDOW) {
                rate_control->num_recent_enc_frames = 0;
                rate_control->sum_recent_enc_size = 0;
            }
            rate_control->sum_recent_enc_size += rate_control->last_enc_size;
            rate_control->num_recent_enc_frames++;
            rate_control->adjusted_fps_num_frames++;
        }
        rate_control->bit_rate_info.sum_enc_size += encoder->rate_control.last_enc_size;
        rate_control->bit_rate_info.num_enc_frames++;
    }
    return encoder->rate_control.last_enc_size;
}

static void mjpeg_encoder_quality_eval_stop(MJpegEncoder *encoder)
{
    MJpegEncoderRateControl *rate_control = &encoder->rate_control;
    uint32_t quality_id;
    uint32_t fps;

    if (!rate_control->during_quality_eval) {
        return;
    }
    switch (rate_control->quality_eval_data.type) {
    case MJPEG_QUALITY_EVAL_TYPE_UPGRADE:
        quality_id = rate_control->quality_eval_data.min_quality_id;
        fps = rate_control->quality_eval_data.min_quality_fps;
        break;
    case MJPEG_QUALITY_EVAL_TYPE_DOWNGRADE:
        quality_id = rate_control->quality_eval_data.max_quality_id;
        fps = rate_control->quality_eval_data.max_quality_fps;
        break;
    case MJPEG_QUALITY_EVAL_TYPE_SET:
        quality_id = MJPEG_QUALITY_SAMPLE_NUM / 2;
        fps = MJPEG_MAX_FPS / 2;
        break;
    default:
        spice_warning("unexected");
        return;
    }
    mjpeg_encoder_reset_quality(encoder, quality_id, fps, 0);
    spice_debug("during quality evaluation: canceling."
                "reset quality to %d fps %d",
                mjpeg_quality_samples[rate_control->quality_id], rate_control->fps);
}

static void mjpeg_encoder_decrease_bit_rate(MJpegEncoder *encoder)
{
    MJpegEncoderRateControl *rate_control = &encoder->rate_control;
    MJpegEncoderBitRateInfo *bit_rate_info = &rate_control->bit_rate_info;
    uint64_t measured_byte_rate;
    uint32_t measured_fps;
    uint64_t decrease_size;

    mjpeg_encoder_quality_eval_stop(encoder);

    rate_control->client_state.max_video_latency = 0;
    rate_control->client_state.max_audio_latency = 0;
    if (rate_control->warmup_start_time) {
        struct timespec time;
        uint64_t now;

        clock_gettime(CLOCK_MONOTONIC, &time);
        now = ((uint64_t) time.tv_sec) * 1000000000 + time.tv_nsec;
        if (now - rate_control->warmup_start_time < MJPEG_WARMUP_TIME*1000*1000) {
            spice_debug("during warmup. ignoring");
            return;
        } else {
            rate_control->warmup_start_time = 0;
        }
    }

    if (bit_rate_info->num_enc_frames > MJPEG_BIT_RATE_EVAL_MIN_NUM_FRAMES ||
        bit_rate_info->num_enc_frames > rate_control->fps) {
        double duration_sec;

        duration_sec = (bit_rate_info->last_frame_time - bit_rate_info->change_start_time);
        duration_sec /= (1000.0 * 1000.0 * 1000.0);
        measured_byte_rate = bit_rate_info->sum_enc_size / duration_sec;
        measured_fps = bit_rate_info->num_enc_frames / duration_sec;
        decrease_size = bit_rate_info->sum_enc_size / bit_rate_info->num_enc_frames;
        spice_debug("bit rate esitimation %.2f (Mbps) fps %u",
                    measured_byte_rate*8/1024.0/1024,
                    measured_fps);
    } else {
        measured_byte_rate = rate_control->byte_rate;
        measured_fps = rate_control->fps;
        decrease_size = measured_byte_rate/measured_fps;
        spice_debug("bit rate not re-estimated %.2f (Mbps) fps %u",
                    measured_byte_rate*8/1024.0/1024,
                    measured_fps);
    }

    measured_byte_rate = MIN(rate_control->byte_rate, measured_byte_rate);

    if (decrease_size >=  measured_byte_rate) {
        decrease_size = measured_byte_rate / 2;
    }

    rate_control->byte_rate = measured_byte_rate - decrease_size;
    bit_rate_info->change_start_time = 0;
    bit_rate_info->change_start_mm_time = 0;
    bit_rate_info->last_frame_time = 0;
    bit_rate_info->num_enc_frames = 0;
    bit_rate_info->sum_enc_size = 0;
    bit_rate_info->was_upgraded = FALSE;

    spice_debug("decrease bit rate %.2f (Mbps)", rate_control->byte_rate * 8 / 1024.0/1024.0);
    mjpeg_encoder_quality_eval_set_downgrade(encoder,
                                             MJPEG_QUALITY_EVAL_REASON_RATE_CHANGE,
                                             rate_control->quality_id,
                                             rate_control->fps);
}

static void mjpeg_encoder_handle_negative_client_stream_report(MJpegEncoder *encoder,
                                                               uint32_t report_end_frame_mm_time)
{
    MJpegEncoderRateControl *rate_control = &encoder->rate_control;

    spice_debug(NULL);

    if ((rate_control->bit_rate_info.change_start_mm_time > report_end_frame_mm_time ||
        !rate_control->bit_rate_info.change_start_mm_time) &&
         !rate_control->bit_rate_info.was_upgraded) {
        spice_debug("ignoring, a downgrade has already occurred later to the report time");
        return;
    }

    mjpeg_encoder_decrease_bit_rate(encoder);
}

static void mjpeg_encoder_increase_bit_rate(MJpegEncoder *encoder)
{
    MJpegEncoderRateControl *rate_control = &encoder->rate_control;
    MJpegEncoderBitRateInfo *bit_rate_info = &rate_control->bit_rate_info;
    uint64_t measured_byte_rate;
    uint32_t measured_fps;
    uint64_t increase_size;


    if (bit_rate_info->num_enc_frames > MJPEG_BIT_RATE_EVAL_MIN_NUM_FRAMES ||
        bit_rate_info->num_enc_frames > rate_control->fps) {
        uint64_t avg_frame_size;
        double duration_sec;

        duration_sec = (bit_rate_info->last_frame_time - bit_rate_info->change_start_time);
        duration_sec /= (1000.0 * 1000.0 * 1000.0);
        measured_byte_rate = bit_rate_info->sum_enc_size / duration_sec;
        measured_fps = bit_rate_info->num_enc_frames / duration_sec;
        avg_frame_size = bit_rate_info->sum_enc_size / bit_rate_info->num_enc_frames;
        spice_debug("bit rate esitimation %.2f (Mbps) defined %.2f"
                    " fps %u avg-frame-size=%.2f (KB)",
                    measured_byte_rate*8/1024.0/1024,
                    rate_control->byte_rate*8/1024.0/1024,
                    measured_fps,
                    avg_frame_size/1024.0);
        increase_size = avg_frame_size;
    } else {
        spice_debug("not enough samples for measuring the bit rate. no change");
        return;
    }


    mjpeg_encoder_quality_eval_stop(encoder);

    if (measured_byte_rate + increase_size < rate_control->byte_rate) {
        spice_debug("measured byte rate is small: not upgrading, just re-evaluating");
    } else {
        rate_control->byte_rate = MIN(measured_byte_rate, rate_control->byte_rate) + increase_size;
    }

    bit_rate_info->change_start_time = 0;
    bit_rate_info->change_start_mm_time = 0;
    bit_rate_info->last_frame_time = 0;
    bit_rate_info->num_enc_frames = 0;
    bit_rate_info->sum_enc_size = 0;
    bit_rate_info->was_upgraded = TRUE;

    spice_debug("increase bit rate %.2f (Mbps)", rate_control->byte_rate * 8 / 1024.0/1024.0);
    mjpeg_encoder_quality_eval_set_upgrade(encoder,
                                           MJPEG_QUALITY_EVAL_REASON_RATE_CHANGE,
                                           rate_control->quality_id,
                                           rate_control->fps);
}
static void mjpeg_encoder_handle_positive_client_stream_report(MJpegEncoder *encoder,
                                                               uint32_t report_start_frame_mm_time)
{
    MJpegEncoderRateControl *rate_control = &encoder->rate_control;
    MJpegEncoderBitRateInfo *bit_rate_info = &rate_control->bit_rate_info;
    int stable_client_mm_time;
    int timeout;

    if (rate_control->during_quality_eval &&
        rate_control->quality_eval_data.reason == MJPEG_QUALITY_EVAL_REASON_RATE_CHANGE) {
        spice_debug("during quality evaluation (rate change). ignoring report");
        return;
    }

    if ((rate_control->fps > MJPEG_IMPROVE_QUALITY_FPS_STRICT_TH ||
         rate_control->fps >= encoder->cbs.get_source_fps(encoder->cbs_opaque)) &&
         rate_control->quality_id > MJPEG_QUALITY_SAMPLE_NUM / 2) {
        timeout = MJPEG_CLIENT_POSITIVE_REPORT_STRICT_TIMEOUT;
    } else {
        timeout = MJPEG_CLIENT_POSITIVE_REPORT_TIMEOUT;
    }

    stable_client_mm_time = (int)report_start_frame_mm_time - bit_rate_info->change_start_mm_time;

    if (!bit_rate_info->change_start_mm_time || stable_client_mm_time < timeout) {
        /* assessing the stability of the current setting and only then
         * respond to the report */
        spice_debug("no drops, but not enough time has passed for assessing"
                    "the playback stability since the last bit rate change");
        return;
    }
    mjpeg_encoder_increase_bit_rate(encoder);
}

/*
 * the video playback jitter buffer should be at least (send_time*2 + net_latency) for
 * preventing underflow
 */
static uint32_t get_min_required_playback_delay(uint64_t frame_enc_size,
                                                uint64_t byte_rate,
                                                uint32_t latency)
{
    uint32_t one_frame_time;
    uint32_t min_delay;

    if (!frame_enc_size || !byte_rate) {
        return latency;
    }
    one_frame_time = (frame_enc_size*1000)/byte_rate;

    min_delay = MIN(one_frame_time*2 + latency, MJPEG_MAX_CLIENT_PLAYBACK_DELAY);
    return min_delay;
}

#define MJPEG_PLAYBACK_LATENCY_DECREASE_FACTOR 0.5
#define MJPEG_VIDEO_VS_AUDIO_LATENCY_FACTOR 1.25
#define MJPEG_VIDEO_DELAY_TH -15

void mjpeg_encoder_client_stream_report(MJpegEncoder *encoder,
                                        uint32_t num_frames,
                                        uint32_t num_drops,
                                        uint32_t start_frame_mm_time,
                                        uint32_t end_frame_mm_time,
                                        int32_t end_frame_delay,
                                        uint32_t audio_delay)
{
    MJpegEncoderRateControl *rate_control = &encoder->rate_control;
    MJpegEncoderClientState *client_state = &rate_control->client_state;
    uint64_t avg_enc_size = 0;
    uint32_t min_playback_delay;
    int is_video_delay_small = FALSE;

    spice_debug("client report: #frames %u, #drops %d, duration %u video-delay %d audio-delay %u",
                num_frames, num_drops,
                end_frame_mm_time - start_frame_mm_time,
                end_frame_delay, audio_delay);

    if (!encoder->rate_control_is_active) {
        spice_debug("rate control was not activated: ignoring");
        return;
    }
    if (rate_control->during_quality_eval) {
        if (rate_control->quality_eval_data.type == MJPEG_QUALITY_EVAL_TYPE_DOWNGRADE &&
            rate_control->quality_eval_data.reason == MJPEG_QUALITY_EVAL_REASON_RATE_CHANGE) {
            spice_debug("during rate downgrade evaluation");
            return;
        }
    }

    if (rate_control->num_recent_enc_frames) {
        avg_enc_size = rate_control->sum_recent_enc_size /
                       rate_control->num_recent_enc_frames;
    }
    spice_debug("recent size avg %.2f (KB)", avg_enc_size / 1024.0);
    min_playback_delay = get_min_required_playback_delay(avg_enc_size, rate_control->byte_rate,
                                                         mjpeg_encoder_get_latency(encoder));
    spice_debug("min-delay %u client-delay %d", min_playback_delay, end_frame_delay);

    if (min_playback_delay > end_frame_delay) {
        uint32_t src_fps = encoder->cbs.get_source_fps(encoder->cbs_opaque);
        /*
        * if the stream is at its highest rate, we can't estimate the "real"
        * network bit rate and the min_playback_delay
        */
        if (rate_control->quality_id != MJPEG_QUALITY_SAMPLE_NUM - 1 ||
            rate_control->fps < MIN(src_fps, MJPEG_MAX_FPS) || end_frame_delay < 0) {
            is_video_delay_small = TRUE;
            if (encoder->cbs.update_client_playback_delay) {
                encoder->cbs.update_client_playback_delay(encoder->cbs_opaque,
                                                          min_playback_delay);
            }
        }
    }


    /*
     * If the audio latency has decreased (since the start of the current
     * sequence of positive reports), and the video latency is bigger, slow down
     * the video rate
     */
    if (end_frame_delay > 0 &&
        audio_delay < MJPEG_PLAYBACK_LATENCY_DECREASE_FACTOR*client_state->max_audio_latency &&
        end_frame_delay > MJPEG_VIDEO_VS_AUDIO_LATENCY_FACTOR*audio_delay) {
        spice_debug("video_latency >> audio_latency && audio_latency << max (%u)",
                    client_state->max_audio_latency);
        mjpeg_encoder_handle_negative_client_stream_report(encoder,
                                                           end_frame_mm_time);
        return;
    }

    if (end_frame_delay < MJPEG_VIDEO_DELAY_TH) {
        mjpeg_encoder_handle_negative_client_stream_report(encoder,
                                                           end_frame_mm_time);
    } else {
        double major_delay_decrease_thresh;
        double medium_delay_decrease_thresh;

        client_state->max_video_latency = MAX(end_frame_delay, client_state->max_video_latency);
        client_state->max_audio_latency = MAX(audio_delay, client_state->max_audio_latency);

        medium_delay_decrease_thresh = client_state->max_video_latency;
        medium_delay_decrease_thresh *= MJPEG_PLAYBACK_LATENCY_DECREASE_FACTOR;

        major_delay_decrease_thresh = medium_delay_decrease_thresh;
        major_delay_decrease_thresh *= MJPEG_PLAYBACK_LATENCY_DECREASE_FACTOR;
        /*
         * since the bit rate and the required latency are only evaluation based on the
         * reports we got till now, we assume that the latency is too low only if it
         * was higher during the time that passed since the last report that resulted
         * in a bit rate decrement. If we find that the latency has decreased, it might
         * suggest that the stream bit rate is too high.
         */
        if ((end_frame_delay < medium_delay_decrease_thresh &&
            is_video_delay_small) || end_frame_delay < major_delay_decrease_thresh) {
            spice_debug("downgrade due to short video delay (last=%u, past-max=%u",
                end_frame_delay, client_state->max_video_latency);
            mjpeg_encoder_handle_negative_client_stream_report(encoder,
                                                               end_frame_mm_time);
        } else if (!num_drops) {
            mjpeg_encoder_handle_positive_client_stream_report(encoder,
                                                               start_frame_mm_time);

        }
    }
}

void mjpeg_encoder_notify_server_frame_drop(MJpegEncoder *encoder)
{
    encoder->rate_control.server_state.num_frames_dropped++;
    mjpeg_encoder_process_server_drops(encoder);
}

/*
 * decrease the bit rate if the drop rate on the sever side exceeds a pre defined
 * threshold.
 */
static void mjpeg_encoder_process_server_drops(MJpegEncoder *encoder)
{
    MJpegEncoderServerState *server_state = &encoder->rate_control.server_state;
    uint32_t num_frames_total;
    double drop_factor;
    uint32_t fps;

    fps = MIN(encoder->rate_control.fps, encoder->cbs.get_source_fps(encoder->cbs_opaque));
    if (server_state->num_frames_encoded < fps * MJPEG_SERVER_STATUS_EVAL_FPS_INTERVAL) {
        return;
    }

    num_frames_total = server_state->num_frames_dropped + server_state->num_frames_encoded;
    drop_factor = ((double)server_state->num_frames_dropped) / num_frames_total;

    spice_debug("#drops %u total %u fps %u src-fps %u",
                server_state->num_frames_dropped,
                num_frames_total,
                encoder->rate_control.fps,
                encoder->cbs.get_source_fps(encoder->cbs_opaque));

    if (drop_factor > MJPEG_SERVER_STATUS_DOWNGRADE_DROP_FACTOR_TH) {
        mjpeg_encoder_decrease_bit_rate(encoder);
    }
    server_state->num_frames_encoded = 0;
    server_state->num_frames_dropped = 0;
}

uint64_t mjpeg_encoder_get_bit_rate(MJpegEncoder *encoder)
{
    return encoder->rate_control.byte_rate * 8;
}

void mjpeg_encoder_get_stats(MJpegEncoder *encoder, MJpegEncoderStats *stats)
{
    spice_assert(encoder != NULL && stats != NULL);
    stats->starting_bit_rate = encoder->starting_bit_rate;
    stats->cur_bit_rate = mjpeg_encoder_get_bit_rate(encoder);
    stats->avg_quality = (double)encoder->avg_quality / encoder->num_frames;
}

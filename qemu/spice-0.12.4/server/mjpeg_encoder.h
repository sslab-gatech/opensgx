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

#ifndef _H_MJPEG_ENCODER
#define _H_MJPEG_ENCODER

#include "red_common.h"

enum {
    MJPEG_ENCODER_FRAME_UNSUPPORTED = -1,
    MJPEG_ENCODER_FRAME_DROP,
    MJPEG_ENCODER_FRAME_ENCODE_START,
};

typedef struct MJpegEncoder MJpegEncoder;

/*
 * Callbacks required for controling and adjusting
 * the stream bit rate:
 * get_roundtrip_ms: roundtrip time in milliseconds
 * get_source_fps: the input frame rate (#frames per second), i.e.,
 * the rate of frames arriving from the guest to spice-server,
 * before any drops.
 */
typedef struct MJpegEncoderRateControlCbs {
    uint32_t (*get_roundtrip_ms)(void *opaque);
    uint32_t (*get_source_fps)(void *opaque);
    void (*update_client_playback_delay)(void *opaque, uint32_t delay_ms);
} MJpegEncoderRateControlCbs;

typedef struct MJpegEncoderStats {
    uint64_t starting_bit_rate;
    uint64_t cur_bit_rate;
    double avg_quality;
} MJpegEncoderStats;

MJpegEncoder *mjpeg_encoder_new(int bit_rate_control, uint64_t starting_bit_rate,
                                MJpegEncoderRateControlCbs *cbs, void *opaque);
void mjpeg_encoder_destroy(MJpegEncoder *encoder);

uint8_t mjpeg_encoder_get_bytes_per_pixel(MJpegEncoder *encoder);

/*
 * dest must be either NULL or allocated by malloc, since it might be freed
 * during the encoding, if its size is too small.
 *
 * return:
 *  MJPEG_ENCODER_FRAME_UNSUPPORTED : frame cannot be encoded
 *  MJPEG_ENCODER_FRAME_DROP        : frame should be dropped. This value can only be returned
 *                                    if mjpeg rate control is active.
 *  MJPEG_ENCODER_FRAME_ENCODE_START: frame encoding started. Continue with
 *                                    mjpeg_encoder_encode_scanline.
 */
int mjpeg_encoder_start_frame(MJpegEncoder *encoder, SpiceBitmapFmt format,
                              int width, int height,
                              uint8_t **dest, size_t *dest_len,
                              uint32_t frame_mm_time);
int mjpeg_encoder_encode_scanline(MJpegEncoder *encoder, uint8_t *src_pixels,
                                  size_t image_width);
size_t mjpeg_encoder_end_frame(MJpegEncoder *encoder);

/*
 * bit rate control
 */

/*
 * Data that should be periodically obtained from the client. The report contains:
 * num_frames         : the number of frames that reached the client during the time
 *                      the report is referring to.
 * num_drops          : the part of the above frames that was dropped by the client due to
 *                      late arrival time.
 * start_frame_mm_time: the mm_time of the first frame included in the report
 * end_frame_mm_time  : the mm_time of the last_frame included in the report
 * end_frame_delay    : (end_frame_mm_time - client_mm_time)
 * audio delay        : the latency of the audio playback.
 *                      If there is no audio playback, set it to MAX_UINT.
 *
 */
void mjpeg_encoder_client_stream_report(MJpegEncoder *encoder,
                                        uint32_t num_frames,
                                        uint32_t num_drops,
                                        uint32_t start_frame_mm_time,
                                        uint32_t end_frame_mm_time,
                                        int32_t end_frame_delay,
                                        uint32_t audio_delay);

/*
 * Notify the encoder each time a frame is dropped due to pipe
 * congestion.
 * We can deduce the client state by the frame dropping rate in the server.
 * Monitoring the frame drops can help in fine tuning the playback parameters
 * when the client reports are delayed.
 */
void mjpeg_encoder_notify_server_frame_drop(MJpegEncoder *encoder);

uint64_t mjpeg_encoder_get_bit_rate(MJpegEncoder *encoder);
void mjpeg_encoder_get_stats(MJpegEncoder *encoder, MJpegEncoderStats *stats);

#endif

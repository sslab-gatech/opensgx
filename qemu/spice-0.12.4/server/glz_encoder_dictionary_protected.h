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

#ifndef _H_GLZ_ENCODER_DICTIONARY_PROTECTED
#define _H_GLZ_ENCODER_DICTIONARY_PROTECTED

/* Interface for using the dictionary for encoding.
   Data structures are exposed for the encoder for efficiency
   purposes. */
typedef struct WindowImage WindowImage;
typedef struct WindowImageSegment WindowImageSegment;


//#define CHAINED_HASH

#ifdef CHAINED_HASH
#define HASH_SIZE_LOG 16
#define HASH_CHAIN_SIZE 4
#else
#define HASH_SIZE_LOG 20
#define HASH_CHAIN_SIZE 1
#endif

#define HASH_SIZE (1 << HASH_SIZE_LOG)
#define HASH_MASK (HASH_SIZE - 1)

typedef struct HashEntry HashEntry;

typedef struct SharedDictionary SharedDictionary;

struct WindowImage {
    uint64_t id;
    LzImageType type;
    int size;                    // in pixels
    uint32_t first_seg;
    GlzUsrImageContext  *usr_context;
    WindowImage*       next;
    uint8_t is_alive;
};

#define MAX_IMAGE_SEGS_NUM (0xffffffff)
#define NULL_IMAGE_SEG_ID MAX_IMAGE_SEGS_NUM
#define INIT_IMAGE_SEGS_NUM 1000

/* Images can be separated into several chunks. The basic unit of the
   dictionary window is one image segment. Each segment is encoded separately.
   An encoded match can refer to only one segment.*/
struct WindowImageSegment {
    WindowImage     *image;
    uint8_t         *lines;
    uint8_t         *lines_end;
    uint32_t pixels_num;            // Number of pixels in the segment
    uint64_t pixels_so_far;         // Total no. pixels passed through the window till this segment.
                                    // NOTE - never use size delta independently. It should
                                    // always be used with respect to a previous size delta
    uint32_t next;
};


struct  __attribute__ ((__packed__)) HashEntry {
    uint32_t image_seg_idx;
    uint32_t ref_pix_idx;
};


struct SharedDictionary {
    struct {
        /* The segments storage. A dynamic array.
           By referring to a segment by its index, instead of address,
           we save space in the hash entries (32bit instead of 64bit) */
        WindowImageSegment  *segs;
        uint32_t segs_quota;

        /* The window is manged as a linked list rather than as a cyclic
           array in order to keep the indices of the segments consistent
           after reallocation */

        /* the window in a resolution of image segments */
        uint32_t used_segs_head;             // the latest head
        uint32_t used_segs_tail;
        uint32_t free_segs_head;

        uint32_t            *encoders_heads; // Holds for each encoder (by id), the window head when
                                             // it started the encoding.
                                             // The head is NULL_IMAGE_SEG_ID when the encoder is
                                             // not encoding.

        /* the window in a resolution of images. But here the head contains the oldest head*/
        WindowImage*        used_images_tail;
        WindowImage*        used_images_head;
        WindowImage*        free_images;

        uint64_t pixels_so_far;
        uint32_t size_limit;                 // max number of pixels in a window (per encoder)
    } window;

    /* Concurrency issues: the reading/writing of each entry field should be atomic.
       It is allowed that the reading/writing of the whole entry won't be atomic,
       since before we access a reference we check its validity*/
#ifdef CHAINED_HASH
    HashEntry htab[HASH_SIZE][HASH_CHAIN_SIZE];
    uint8_t htab_counter[HASH_SIZE];  //cyclic counter for the next entry in a chain to be assigned
#else
    HashEntry htab[HASH_SIZE];
#endif

    uint64_t last_image_id;
    uint32_t max_encoders;
    pthread_mutex_t lock;
    pthread_rwlock_t rw_alloc_lock;
    GlzEncoderUsrContext       *cur_usr; // each encoder has other context.
};

/*
    Add the image to the tail of the window.
    If possible, release images from the head of the window.
    Also perform concurrency related operations.

    usr_image_context: when an image is released from the window due to capacity overflow,
                       usr_image_context is given as a parameter to the free_image callback.

    image_head_dist  : the number of images between the current image and the head of the
                       window that is associated with the encoder.
*/
WindowImage *glz_dictionary_pre_encode(uint32_t encoder_id, GlzEncoderUsrContext *usr,
                                       SharedDictionary *dict, LzImageType image_type,
                                       int image_width, int image_height, int image_stride,
                                       uint8_t *first_lines, unsigned int num_first_lines,
                                       GlzUsrImageContext *usr_image_context,
                                       uint32_t *image_head_dist);

/*
   Performs concurrency related operations.
   If possible, release images from the head of the window.
*/
void glz_dictionary_post_encode(uint32_t encoder_id, GlzEncoderUsrContext *usr,
                                SharedDictionary *dict);

#define IMAGE_SEG_IS_EARLIER(dict, dst_seg, src_seg) (                     \
    ((src_seg) == NULL_IMAGE_SEG_ID) || (((dst_seg) != NULL_IMAGE_SEG_ID)  \
    && ((dict)->window.segs[(dst_seg)].pixels_so_far <                     \
       (dict)->window.segs[(src_seg)].pixels_so_far)))


#ifdef CHAINED_HASH
#define UPDATE_HASH(dict, hval, seg, pix) {                \
    uint8_t tmp_count = (dict)->htab_counter[hval];        \
    (dict)->htab[hval][tmp_count].image_seg_idx = seg;     \
    (dict)->htab[hval][tmp_count].ref_pix_idx = pix;       \
    tmp_count = ((tmp_count) + 1) & (HASH_CHAIN_SIZE - 1); \
    dict->htab_counter[hval] = tmp_count;                  \
}
#else
#define UPDATE_HASH(dict, hval, seg, pix) { \
    (dict)->htab[hval].image_seg_idx = seg; \
    (dict)->htab[hval].ref_pix_idx = pix;   \
}
#endif

/* checks if the reference segment is located in the range of the window
   of the current encoder */
#define REF_SEG_IS_VALID(dict, enc_id, ref_seg, src_seg) ( \
    ((ref_seg) == (src_seg)) ||                            \
    ((ref_seg)->image &&                                   \
     (ref_seg)->image->is_alive &&                         \
     (src_seg->image->type == ref_seg->image->type) &&     \
     (ref_seg->pixels_so_far <= src_seg->pixels_so_far) && \
     ((dict)->window.segs[                                 \
        (dict)->window.encoders_heads[enc_id]].pixels_so_far <= \
        ref_seg->pixels_so_far)))

#endif // _H_GLZ_ENCODER_DICTIONARY_PROTECTED

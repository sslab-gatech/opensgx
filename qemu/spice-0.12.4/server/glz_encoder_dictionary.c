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

#include <pthread.h>
#include <string.h>
#include <stdio.h>

#include "glz_encoder_dictionary.h"
#include "glz_encoder_dictionary_protected.h"

/* turning all used images to free ones. If they are alive, calling the free_image callback for
   each one */
static INLINE void __glz_dictionary_window_reset_images(SharedDictionary *dict)
{
    WindowImage *tmp;

    while (dict->window.used_images_head) {
        tmp = dict->window.used_images_head;
        dict->window.used_images_head = dict->window.used_images_head->next;
        if (tmp->is_alive) {
            dict->cur_usr->free_image(dict->cur_usr, tmp->usr_context);
        }
        tmp->next = dict->window.free_images;
        tmp->is_alive = FALSE;
        dict->window.free_images = tmp;
    }
    dict->window.used_images_tail = NULL;
}

/* allocate window fields (no reset)*/
static int glz_dictionary_window_create(SharedDictionary *dict, uint32_t size)
{
    if (size > LZ_MAX_WINDOW_SIZE) {
        return FALSE;
    }

    dict->window.size_limit = size;
    dict->window.segs = (WindowImageSegment *)(
            dict->cur_usr->malloc(dict->cur_usr, sizeof(WindowImageSegment) * INIT_IMAGE_SEGS_NUM));

    if (!dict->window.segs) {
        return FALSE;
    }

    dict->window.segs_quota = INIT_IMAGE_SEGS_NUM;

    dict->window.encoders_heads = (uint32_t *)dict->cur_usr->malloc(dict->cur_usr,
                                                            sizeof(uint32_t) * dict->max_encoders);

    if (!dict->window.encoders_heads) {
        dict->cur_usr->free(dict->cur_usr, dict->window.segs);
        return FALSE;
    }

    dict->window.used_images_head = NULL;
    dict->window.used_images_tail = NULL;
    dict->window.free_images = NULL;
    dict->window.pixels_so_far = 0;

    return TRUE;
}

/* initializes an empty window (segs and encoder_heads should be pre allocated.
   resets the image infos, and calls the free_image usr callback*/
static void glz_dictionary_window_reset(SharedDictionary *dict)
{
    uint32_t i;
    WindowImageSegment *seg, *last_seg;

    last_seg = dict->window.segs + dict->window.segs_quota;
    /* reset free segs list */
    dict->window.free_segs_head = 0;
    for (seg = dict->window.segs, i = 0; seg < last_seg; seg++, i++) {
        seg->next = i + 1;
        seg->image = NULL;
        seg->lines = NULL;
        seg->lines_end = NULL;
        seg->pixels_num = 0;
        seg->pixels_so_far = 0;
    }
    dict->window.segs[dict->window.segs_quota - 1].next = NULL_IMAGE_SEG_ID;

    dict->window.used_segs_head = NULL_IMAGE_SEG_ID;
    dict->window.used_segs_tail = NULL_IMAGE_SEG_ID;

    // reset encoders heads
    for (i = 0; i < dict->max_encoders; i++) {
        dict->window.encoders_heads[i] = NULL_IMAGE_SEG_ID;
    }

    __glz_dictionary_window_reset_images(dict);
}

static INLINE void glz_dictionary_reset_hash(SharedDictionary *dict)
{
    memset(dict->htab, 0, sizeof(HashEntry) * HASH_SIZE * HASH_CHAIN_SIZE);
#ifdef CHAINED_HASH
    memset(dict->htab_counter, 0, HASH_SIZE * sizeof(uint8_t));
#endif
}

static INLINE void glz_dictionary_window_destroy(SharedDictionary *dict)
{
    __glz_dictionary_window_reset_images(dict);

    if (dict->window.segs) {
        dict->cur_usr->free(dict->cur_usr, dict->window.segs);
        dict->window.segs = NULL;
    }

    while (dict->window.free_images) {
        WindowImage *tmp = dict->window.free_images;
        dict->window.free_images = tmp->next;

        dict->cur_usr->free(dict->cur_usr, tmp);
    }

    if (dict->window.encoders_heads) {
        dict->cur_usr->free(dict->cur_usr, dict->window.encoders_heads);
        dict->window.encoders_heads = NULL;
    }
}

/* logic removal only */
static INLINE void glz_dictionary_window_kill_image(SharedDictionary *dict, WindowImage *image)
{
    image->is_alive = FALSE;
}

GlzEncDictContext *glz_enc_dictionary_create(uint32_t size, uint32_t max_encoders,
                                             GlzEncoderUsrContext *usr)
{
    SharedDictionary *dict;

    if (!(dict = (SharedDictionary *)usr->malloc(usr,
                                                 sizeof(SharedDictionary)))) {
        return NULL;
    }

    dict->cur_usr = usr;
    dict->last_image_id = 0;
    dict->max_encoders = max_encoders;

    pthread_mutex_init(&dict->lock, NULL);
    pthread_rwlock_init(&dict->rw_alloc_lock, NULL);

    dict->window.encoders_heads = NULL;

    // alloc window fields and reset
    if (!glz_dictionary_window_create(dict, size)) {
        dict->cur_usr->free(usr, dict);
        return NULL;
    }

    // reset window and hash
    glz_enc_dictionary_reset((GlzEncDictContext *)dict, usr);

    return (GlzEncDictContext *)dict;
}

void glz_enc_dictionary_get_restore_data(GlzEncDictContext *opaque_dict,
                                         GlzEncDictRestoreData *out_data, GlzEncoderUsrContext *usr)
{
    SharedDictionary *dict = (SharedDictionary *)opaque_dict;
    dict->cur_usr = usr;
    GLZ_ASSERT(dict->cur_usr, opaque_dict);
    GLZ_ASSERT(dict->cur_usr, out_data);

    out_data->last_image_id = dict->last_image_id;
    out_data->max_encoders = dict->max_encoders;
    out_data->size = dict->window.size_limit;
}

GlzEncDictContext *glz_enc_dictionary_restore(GlzEncDictRestoreData *restore_data,
                                              GlzEncoderUsrContext *usr)
{
    if (!restore_data) {
        return NULL;
    }
    SharedDictionary *ret = (SharedDictionary *)glz_enc_dictionary_create(
            restore_data->size, restore_data->max_encoders, usr);
    ret->last_image_id = restore_data->last_image_id;
    return ((GlzEncDictContext *)ret);
}

void glz_enc_dictionary_reset(GlzEncDictContext *opaque_dict, GlzEncoderUsrContext *usr)
{
    SharedDictionary *dict = (SharedDictionary *)opaque_dict;
    dict->cur_usr = usr;
    GLZ_ASSERT(dict->cur_usr, opaque_dict);

    dict->last_image_id = 0;
    glz_dictionary_window_reset(dict);
    glz_dictionary_reset_hash(dict);
}

void glz_enc_dictionary_destroy(GlzEncDictContext *opaque_dict, GlzEncoderUsrContext *usr)
{
    SharedDictionary *dict = (SharedDictionary *)opaque_dict;

    if (!opaque_dict) {
        return;
    }

    dict->cur_usr = usr;
    glz_dictionary_window_destroy(dict);

    pthread_mutex_destroy(&dict->lock);
    pthread_rwlock_destroy(&dict->rw_alloc_lock);

    dict->cur_usr->free(dict->cur_usr, dict);
}

uint32_t glz_enc_dictionary_get_size(GlzEncDictContext *opaque_dict)
{
    SharedDictionary *dict = (SharedDictionary *)opaque_dict;

    if (!opaque_dict) {
        return 0;
    }
    return dict->window.size_limit;
}

/* doesn't call the remove image callback */
void glz_enc_dictionary_remove_image(GlzEncDictContext *opaque_dict,
                                     GlzEncDictImageContext *opaque_image,
                                     GlzEncoderUsrContext *usr)
{
    SharedDictionary *dict = (SharedDictionary *)opaque_dict;
    WindowImage *image = (WindowImage *)opaque_image;
    dict->cur_usr = usr;
    GLZ_ASSERT(dict->cur_usr, opaque_image && opaque_dict);

    glz_dictionary_window_kill_image(dict, image);
}

/***********************************************************************************
 Mutators of the window. Should be called by the encoder before and after encoding.
 ***********************************************************************************/

static INLINE int __get_pixels_num(LzImageType image_type, unsigned int num_lines, int stride)
{
    if (IS_IMAGE_TYPE_RGB[image_type]) {
        return num_lines * stride / RGB_BYTES_PER_PIXEL[image_type];
    } else {
        return num_lines * stride * PLT_PIXELS_PER_BYTE[image_type];
    }
}

static void __glz_dictionary_window_segs_realloc(SharedDictionary *dict)
{
    WindowImageSegment *new_segs;
    uint32_t new_quota = (MAX_IMAGE_SEGS_NUM < (dict->window.segs_quota * 2)) ?
        MAX_IMAGE_SEGS_NUM : (dict->window.segs_quota * 2);
    WindowImageSegment *seg;
    uint32_t i;

    pthread_rwlock_wrlock(&dict->rw_alloc_lock);

    if (dict->window.segs_quota == MAX_IMAGE_SEGS_NUM) {
        dict->cur_usr->error(dict->cur_usr, "overflow in image segments window\n");
    }

    new_segs = (WindowImageSegment*)dict->cur_usr->malloc(
            dict->cur_usr, sizeof(WindowImageSegment) * new_quota);

    if (!new_segs) {
        dict->cur_usr->error(dict->cur_usr,
                             "realloc of dictionary window failed\n");
    }

    memcpy(new_segs, dict->window.segs,
           sizeof(WindowImageSegment) * dict->window.segs_quota);

    // reseting the new elements
    for (i = dict->window.segs_quota, seg = new_segs + i; i < new_quota; i++, seg++) {
        seg->image = NULL;
        seg->lines = NULL;
        seg->lines_end = NULL;
        seg->pixels_num = 0;
        seg->pixels_so_far = 0;
        seg->next = i + 1;
    }
    new_segs[new_quota - 1].next = dict->window.free_segs_head;
    dict->window.free_segs_head = dict->window.segs_quota;

    dict->cur_usr->free(dict->cur_usr, dict->window.segs);
    dict->window.segs = new_segs;
    dict->window.segs_quota = new_quota;

    pthread_rwlock_unlock(&dict->rw_alloc_lock);
}

/* NOTE - it also updates the used_images_list*/
static WindowImage *__glz_dictionary_window_alloc_image(SharedDictionary *dict)
{
    WindowImage *ret;

    if (dict->window.free_images) {
        ret = dict->window.free_images;
        dict->window.free_images = ret->next;
    } else {
        if (!(ret = (WindowImage *)dict->cur_usr->malloc(dict->cur_usr,
                                                         sizeof(*ret)))) {
            return NULL;
        }
    }

    ret->next = NULL;
    if (dict->window.used_images_tail) {
        dict->window.used_images_tail->next = ret;
    }
    dict->window.used_images_tail = ret;

    if (!dict->window.used_images_head) {
        dict->window.used_images_head = ret;
    }
    return ret;
}

/* NOTE - it doesn't update the used_segs list*/
static uint32_t __glz_dictionary_window_alloc_image_seg(SharedDictionary *dict)
{
    uint32_t seg_id;
    WindowImageSegment *seg;

    // TODO: when is it best to realloc? when full or when half full?
    if (dict->window.free_segs_head == NULL_IMAGE_SEG_ID) {
        __glz_dictionary_window_segs_realloc(dict);
    }

    GLZ_ASSERT(dict->cur_usr, dict->window.free_segs_head != NULL_IMAGE_SEG_ID);

    seg_id = dict->window.free_segs_head;
    seg = dict->window.segs + seg_id;
    dict->window.free_segs_head = seg->next;

    return seg_id;
}

/* moves image to free list and "kill" it. Calls the free_image callback if was alive. */
static INLINE void __glz_dictionary_window_free_image(SharedDictionary *dict, WindowImage *image)
{
    if (image->is_alive) {
        dict->cur_usr->free_image(dict->cur_usr, image->usr_context);
    }
    image->is_alive = FALSE;
    image->next = dict->window.free_images;
    dict->window.free_images = image;
}

/* moves all the segments that were associated with the images to the free segments */
static INLINE void __glz_dictionary_window_free_image_segs(SharedDictionary *dict,
                                                           WindowImage *image)
{
    uint32_t old_free_head = dict->window.free_segs_head;
    uint32_t seg_id, next_seg_id;

    GLZ_ASSERT(dict->cur_usr, image->first_seg != NULL_IMAGE_SEG_ID);
    dict->window.free_segs_head = image->first_seg;

    // retrieving the last segment of the image
    for (seg_id = image->first_seg, next_seg_id = dict->window.segs[seg_id].next;
         (next_seg_id != NULL_IMAGE_SEG_ID) && (dict->window.segs[next_seg_id].image == image);
         seg_id = next_seg_id, next_seg_id = dict->window.segs[seg_id].next) {
    }

    // concatenate the free list
    dict->window.segs[seg_id].next = old_free_head;
}

/* Returns the logical head of the window after we add an image with the give size to its tail.
   Returns NULL when the window is empty, of when we have to empty the window in order
   to insert the new image. */
static WindowImage *glz_dictionary_window_get_new_head(SharedDictionary *dict, int new_image_size)
{
    uint32_t cur_win_size;
    WindowImage *cur_head;

    if ((uint32_t)new_image_size > dict->window.size_limit) {
        dict->cur_usr->error(dict->cur_usr, "image is bigger than window\n");
    }

    GLZ_ASSERT(dict->cur_usr, new_image_size < dict->window.size_limit)

    // the window is empty
    if (!dict->window.used_images_head) {
        return NULL;
    }

    GLZ_ASSERT(dict->cur_usr, dict->window.used_segs_head != NULL_IMAGE_SEG_ID);
    GLZ_ASSERT(dict->cur_usr, dict->window.used_segs_tail != NULL_IMAGE_SEG_ID);

    // used_segs_head is the latest logical head (the physical head may preceed it)
    cur_head = dict->window.segs[dict->window.used_segs_head].image;
    cur_win_size = dict->window.segs[dict->window.used_segs_tail].pixels_num +
        dict->window.segs[dict->window.used_segs_tail].pixels_so_far -
        dict->window.segs[dict->window.used_segs_head].pixels_so_far;

    while ((cur_win_size + new_image_size) > dict->window.size_limit) {
        GLZ_ASSERT(dict->cur_usr, cur_head);
        cur_win_size -= cur_head->size;
        cur_head = cur_head->next;
    }

    return cur_head;
}

static INLINE int glz_dictionary_is_in_use(SharedDictionary *dict)
{
    uint32_t i = 0;
    for (i = 0; i < dict->max_encoders; i++) {
        if (dict->window.encoders_heads[i] != NULL_IMAGE_SEG_ID) {
            return TRUE;
        }
    }
    return FALSE;
}

/* remove from the window (and free relevant data) the images between the oldest physical head
   (inclusive) and the end_image (exclusive). If end_image is NULL, empties the window*/
static void glz_dictionary_window_remove_head(SharedDictionary *dict, uint32_t encoder_id,
                                              WindowImage *end_image)
{
    // note that the segs list heads (one per encoder) may be different than the
    // used_segs_head and it is updated somewhere else
    while (dict->window.used_images_head != end_image) {
        WindowImage *image = dict->window.used_images_head;

        __glz_dictionary_window_free_image_segs(dict, image);
        dict->window.used_images_head = image->next;
        __glz_dictionary_window_free_image(dict, image);
    }

    if (!dict->window.used_images_head) {
        dict->window.used_segs_head = NULL_IMAGE_SEG_ID;
        dict->window.used_segs_tail = NULL_IMAGE_SEG_ID;
        dict->window.used_images_tail = NULL;
    } else {
        dict->window.used_segs_head = end_image->first_seg;
    }
}

static uint32_t glz_dictionary_window_alloc_image_seg(SharedDictionary *dict, WindowImage* image,
                                                      int size, int stride,
                                                      uint8_t *lines, unsigned int num_lines)
{
    uint32_t seg_id = __glz_dictionary_window_alloc_image_seg(dict);
    WindowImageSegment *seg = &dict->window.segs[seg_id];

    seg->image = image;
    seg->lines = lines;
    seg->lines_end = lines + num_lines * stride;
    seg->pixels_num = size;
    seg->pixels_so_far = dict->window.pixels_so_far;
    dict->window.pixels_so_far += seg->pixels_num;

    seg->next = NULL_IMAGE_SEG_ID;

    return seg_id;
}

static WindowImage *glz_dictionary_window_add_image(SharedDictionary *dict, LzImageType image_type,
                                                    int image_size, int image_height,
                                                    int image_stride, uint8_t *first_lines,
                                                    unsigned int num_first_lines,
                                                    GlzUsrImageContext *usr_image_context)
{
    unsigned int num_lines = num_first_lines;
    unsigned int row;
    uint32_t seg_id, prev_seg_id;
    uint8_t* lines = first_lines;
    // alloc image info,update used head tail,  if used_head null - update  head
    WindowImage *image = __glz_dictionary_window_alloc_image(dict);
    image->id = dict->last_image_id++;
    image->size = image_size;
    image->type = image_type;
    image->usr_context = usr_image_context;

    if (num_lines <= 0) {
        num_lines = dict->cur_usr->more_lines(dict->cur_usr, &lines);
        if (num_lines <= 0) {
            dict->cur_usr->error(dict->cur_usr, "more lines failed\n");
        }
    }

    for (row = 0;;) {
        seg_id = glz_dictionary_window_alloc_image_seg(dict, image,
                                                       image_size * num_lines / image_height,
                                                       image_stride,
                                                       lines, num_lines);
        if (row == 0) {
            image->first_seg = seg_id;
        } else {
            dict->window.segs[prev_seg_id].next = seg_id;
        }

        row += num_lines;
        if (row < (uint32_t)image_height) {
            num_lines = dict->cur_usr->more_lines(dict->cur_usr, &lines);
            if (num_lines <= 0) {
                dict->cur_usr->error(dict->cur_usr, "more lines failed\n");
            }
        } else {
            break;
        }
        prev_seg_id = seg_id;
    }

    if (dict->window.used_segs_tail == NULL_IMAGE_SEG_ID) {
        dict->window.used_segs_head = image->first_seg;
        dict->window.used_segs_tail = seg_id;
    } else {
        int prev_tail = dict->window.used_segs_tail;

        // The used segs may be in use by another thread which is during encoding
        // (read-only use - when going over the segs of an image,
        // see glz_encode_tmpl::compress).
        // Thus, the 'next' field of the list's tail can be accessed only
        // after all the new tail's data was set. Note that we are relying on
        // an atomic assignment (32 bit variable).
        // For the other thread that may read 'next' of the old tail, NULL_IMAGE_SEG_ID
        // is equivalent to a segment with an image id that is different
        // from the image id of the tail, so we don't need to further protect this field.
        dict->window.segs[prev_tail].next = image->first_seg;
        dict->window.used_segs_tail = seg_id;
    }
    image->is_alive = TRUE;

    return image;
}

WindowImage *glz_dictionary_pre_encode(uint32_t encoder_id, GlzEncoderUsrContext *usr,
                                       SharedDictionary *dict, LzImageType image_type,
                                       int image_width, int image_height, int image_stride,
                                       uint8_t *first_lines, unsigned int num_first_lines,
                                       GlzUsrImageContext *usr_image_context,
                                       uint32_t *image_head_dist)
{
    WindowImage *new_win_head, *ret;
    int image_size;


    pthread_mutex_lock(&dict->lock);

    dict->cur_usr = usr;
    GLZ_ASSERT(dict->cur_usr, dict->window.encoders_heads[encoder_id] == NULL_IMAGE_SEG_ID);

    image_size = __get_pixels_num(image_type, image_height, image_stride);
    new_win_head = glz_dictionary_window_get_new_head(dict, image_size);

    if (!glz_dictionary_is_in_use(dict)) {
        glz_dictionary_window_remove_head(dict, encoder_id, new_win_head);
    }

    ret = glz_dictionary_window_add_image(dict, image_type, image_size, image_height, image_stride,
                                          first_lines, num_first_lines, usr_image_context);

    if (new_win_head) {
        dict->window.encoders_heads[encoder_id] = new_win_head->first_seg;
        *image_head_dist = (uint32_t)(ret->id - new_win_head->id); // shouldn't be greater than 32
                                                                   // bit because the window size is
                                                                   // limited to 2^25
    } else {
        dict->window.encoders_heads[encoder_id] = ret->first_seg;
        *image_head_dist = 0;
    }


    // update encoders head  (the other heads were already updated)
    pthread_mutex_unlock(&dict->lock);
    pthread_rwlock_rdlock(&dict->rw_alloc_lock);
    return ret;
}

void glz_dictionary_post_encode(uint32_t encoder_id, GlzEncoderUsrContext *usr,
                                SharedDictionary *dict)
{
    uint32_t i;
    uint32_t early_head_seg = NULL_IMAGE_SEG_ID;
    uint32_t this_encoder_head_seg;

    pthread_rwlock_unlock(&dict->rw_alloc_lock);
    pthread_mutex_lock(&dict->lock);
    dict->cur_usr = usr;

    GLZ_ASSERT(dict->cur_usr, dict->window.encoders_heads[encoder_id] != NULL_IMAGE_SEG_ID);
    // get the earliest head in use (not including this encoder head)
    for (i = 0; i < dict->max_encoders; i++) {
        if (i != encoder_id) {
            if (IMAGE_SEG_IS_EARLIER(dict, dict->window.encoders_heads[i], early_head_seg)) {
                early_head_seg = dict->window.encoders_heads[i];
            }
        }
    }

    // possible only if early_head_seg == NULL
    if (IMAGE_SEG_IS_EARLIER(dict, dict->window.used_segs_head, early_head_seg)) {
        early_head_seg = dict->window.used_segs_head;
    }

    this_encoder_head_seg = dict->window.encoders_heads[encoder_id];

    GLZ_ASSERT(dict->cur_usr, early_head_seg != NULL_IMAGE_SEG_ID);

    if (IMAGE_SEG_IS_EARLIER(dict, this_encoder_head_seg, early_head_seg)) {
        GLZ_ASSERT(dict->cur_usr,
                   this_encoder_head_seg == dict->window.used_images_head->first_seg);
        glz_dictionary_window_remove_head(dict, encoder_id,
                                          dict->window.segs[early_head_seg].image);
    }


    dict->window.encoders_heads[encoder_id] = NULL_IMAGE_SEG_ID;
    pthread_mutex_unlock(&dict->lock);
}

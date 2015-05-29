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

#ifndef _H_GLZ_DECODER_WINDOW
#define _H_GLZ_DECODER_WINDOW

#include "glz_decoder_config.h"
#include "glz_decoded_image.h"

#include <list>

#include "read_write_mutex.h"

typedef int DecodedImageWinId;

/*
    The class represents the lz window of images, which is shared among the glz decoders
*/

class GlzDecoderWindow {
public:
    GlzDecoderWindow(GlzDecoderDebug &debug_calls);
    virtual ~GlzDecoderWindow();

    DecodedImageWinId pre_decode(uint64_t image_id, uint64_t relative_head_id);

    void post_decode(GlzDecodedImage *image);

    /* NOTE - get_ref_pixel should be called only after pre_decode was called and
       before post decode was called */
    uint8_t *get_ref_pixel(DecodedImageWinId decoded_image_win_id, int dist_from_ref_image,
                           int pixel_offset);

    void abort();

    /* NOTE - clear mustn't be called if the window is currently used by a decoder*/
    void clear();

private:
    void wait_for_image(int index);
    void add_image(GlzDecodedImage *image);
    uint8_t* get_pixel_after_image_entered(int image_index, int pixel_offset);

    bool will_overflow(uint64_t image_id, uint64_t relative_head_id);
    bool is_empty();

    DecodedImageWinId pre_decode_update_window(uint64_t image_id, uint64_t relative_head_id);
    void pre_decode_finalize();
    void post_decode_intialize();
    void post_decode_update_window(GlzDecodedImage *image);
    void add_pre_decoded_image(uint64_t image_id);
    void add_decoded_image(GlzDecodedImage *image);
    void narrow_window(GlzDecodedImage *last_added);
    void remove_head(uint64_t new_head_image_id);
    int  calc_image_win_idx(uint64_t image_id);
    int  calc_realloc_size(uint64_t new_tail_image_id);
    void realloc(int size);
    void init();
    void release_images();

private:
    GlzDecodedImage **_images; // cyclic window
    int _head_idx;            // index in images array (not image id)
    uint64_t _tail_image_id;
    int _images_capacity;
    int _n_images;            // _n_images counts all the images in
                              // the window, including the missing ones

    std::list<uint64_t> _missing_list;

    Mutex _win_modifiers_mutex;
    ReadWriteMutex _win_alloc_rw_mutex;
    Mutex _new_image_mutex;

    Condition _new_image_cond;          // when get_pixel_ref waits for an image
    Condition _release_image_cond;      // when waiting for the window to narrow.
    Condition _win_alloc_cond;

    bool _aborting;

    GlzDecoderDebug &_debug_calls;
};

inline uint8_t* GlzDecoderWindow::get_pixel_after_image_entered(int image_index,
                                                                int pixel_offset)
{
    return _images[image_index]->get_pixel_ref(pixel_offset);
}

/* should be called only between pre and post (when realloc mutex is write-locked).
   Note that it can't use calc_image_win_idx, since the window is not locked for changes
   (that are not reallocation) during decoding.*/
inline uint8_t *GlzDecoderWindow::get_ref_pixel(DecodedImageWinId decoded_image_win_id,
                                                int dist_from_ref_image, int pixel_offset)
{
    int ref_image_index = (dist_from_ref_image <= decoded_image_win_id) ?
        (decoded_image_win_id - dist_from_ref_image) :
        _images_capacity + (decoded_image_win_id - dist_from_ref_image);

    if (_images[ref_image_index] == NULL) { // reading image is atomic
        wait_for_image(ref_image_index);
    }

    // after image entered - it won't leave the window till no decoder needs it
    return get_pixel_after_image_entered(ref_image_index, pixel_offset);
}

#endif // _H_GLZ_DECODER_WINDOW

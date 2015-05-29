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

#include "common.h"

#include "glz_decoder_window.h"
#include "utils.h"

#define INIT_IMAGES_CAPACITY 100
#define WIN_REALLOC_FACTOR 1.5

GlzDecoderWindow::GlzDecoderWindow(GlzDecoderDebug &debug_calls)
    : _aborting (false)
    , _debug_calls (debug_calls)
{
    _images_capacity = INIT_IMAGES_CAPACITY;
    _images = new  GlzDecodedImage*[_images_capacity];
    if (!_images) {
        _debug_calls.error(std::string("failed allocating images\n"));
    }

    memset(_images, 0, sizeof(GlzDecodedImage*) * _images_capacity);

    init();
}

GlzDecoderWindow::~GlzDecoderWindow()
{
    clear();
    delete[] _images;
}

DecodedImageWinId GlzDecoderWindow::pre_decode(uint64_t image_id, uint64_t relative_head_id)
{
    DecodedImageWinId image_win_id = pre_decode_update_window(image_id, relative_head_id);
    pre_decode_finalize();
    return image_win_id;
}

void GlzDecoderWindow::post_decode(GlzDecodedImage *image)
{
    post_decode_intialize();
    post_decode_update_window(image);
}

/* index: the physical index in the images array. Note that it can't change between waits since
   the realloc mutex should be read locked.
   No starvation for the realloc mutex can occur, since the image we wait for is located before us,
   hence, when it arrives - no realloc is needed. */
void GlzDecoderWindow::wait_for_image(int index)
{
    Lock lock(_new_image_mutex);
    GlzDecodedImage *image = _images[index]; // can be performed without locking the _win_mutex,
                                             // since it is called after pre and the rw mutex is                                                 // locked, hence, physical changes to the window are
                                             // not allowed. In addition the reading of the image ptr
                                             // is atomic, thus, even if the value changes we are
                                             // not affected.

    while (!image) {
        if (_aborting) {
            THROW("aborting");
        }
        _new_image_cond.wait(lock);
        image = _images[index];
    }
}

void GlzDecoderWindow::abort()
{
    Lock lock1(_win_modifiers_mutex);
    Lock lock2(_new_image_mutex);
    _aborting = true;
    _new_image_cond.notify_all();
    _release_image_cond.notify_all();
    _win_alloc_cond.notify_all();
}

void GlzDecoderWindow::clear()
{
    Lock lock(_win_modifiers_mutex);
    release_images();
    init();
}

void GlzDecoderWindow::init()
{
    _missing_list.clear();
    // The window is never empty: the head is in the missing list or in the window.
    // The first missing image is 0.
    _missing_list.push_front(0);
    _head_idx = 0;
    _tail_image_id = 0;
    _n_images = 1;
}

void GlzDecoderWindow::release_images()
{
    for (int i = 0; i < _n_images; i++) {
        int idx = (_head_idx + i) % _images_capacity;
        if (_images[idx]) {
            delete _images[idx];
            _images[idx] = NULL;
        }
    }
}

inline bool GlzDecoderWindow::is_empty()
{
    return (!_n_images);
}

inline bool GlzDecoderWindow::will_overflow(uint64_t image_id, uint64_t relative_head_id)
{
    if (image_id <= _tail_image_id) {
        return false;
    }

    if (!_missing_list.empty() && (_missing_list.front() < relative_head_id)) {
        // two non overlapping windows
        return true;
    }

    return false;
}

DecodedImageWinId GlzDecoderWindow::pre_decode_update_window(uint64_t image_id,
                                                             uint64_t relative_head_id)
{
    Lock lock(_win_modifiers_mutex);
    int realloc_size;

    while (will_overflow(image_id, relative_head_id)) {
        if (_aborting) {
            THROW("aborting");
        }
        _release_image_cond.wait(lock);
    }

    // The following conditions prevent starvation in case thread (1) should realloc,
    // thread (2) is during decode and waits for a previous image and
    /// thread (3) should decode this the previous image and needs to enter pre decode although
    // (1) is in there.
    // We must give priority to older images in the window.
    // The condition should be checked over again in case a later image entered the window
    // and the realocation was already performed
    while ((realloc_size = calc_realloc_size(image_id))) {
        if (_aborting) {
            THROW("aborting");
        }

        if (_win_alloc_rw_mutex.try_write_lock()) {
            realloc(realloc_size);
            _win_alloc_rw_mutex.write_unlock();
            break;
        } else {
            _win_alloc_cond.wait(lock);
        }
    }

    if (image_id > _tail_image_id) { // not in missing list
        add_pre_decoded_image(image_id);
    }

    return calc_image_win_idx(image_id);
}

void GlzDecoderWindow::add_pre_decoded_image(uint64_t image_id)
{
    GLZ_ASSERT(_debug_calls, image_id > _tail_image_id);
    GLZ_ASSERT(_debug_calls, image_id - _tail_image_id + _n_images < _images_capacity);

    for (uint64_t miss_id = _tail_image_id + 1; miss_id <= image_id; miss_id++) {
        _missing_list.push_back(miss_id);
        _n_images++;
    }
    _tail_image_id = image_id;
}

inline int GlzDecoderWindow::calc_realloc_size(uint64_t new_tail_image_id)
{
    if (new_tail_image_id <= _tail_image_id) {
        return 0;
    }

    int min_capacity = (int)(_n_images + new_tail_image_id - _tail_image_id);

    if ((min_capacity * WIN_REALLOC_FACTOR) > _images_capacity) {
        return (int)MAX(min_capacity * WIN_REALLOC_FACTOR, WIN_REALLOC_FACTOR * _images_capacity);
    } else {
        return 0;
    }
}

void GlzDecoderWindow::realloc(int size)
{
    GlzDecodedImage **new_images = new GlzDecodedImage*[size];

    if (!new_images) {
        _debug_calls.error(std::string("failed allocating images array"));
    }
    memset(new_images, 0, sizeof(GlzDecodedImage*) * size);

    for (int i = 0; i < _n_images; i++) {
        new_images[i] = _images[(i + _head_idx) % _images_capacity];
    }
    delete[] _images;

    _images = new_images;
    _head_idx = 0;
    _images_capacity = size;
}

void GlzDecoderWindow::pre_decode_finalize()
{
    _win_alloc_rw_mutex.read_lock();
}

void GlzDecoderWindow::post_decode_intialize()
{
    _win_alloc_rw_mutex.read_unlock();
}

void GlzDecoderWindow::post_decode_update_window(GlzDecodedImage *image)
{
    Lock lock(_win_modifiers_mutex);
    add_decoded_image(image);
    narrow_window(image);
    _win_alloc_cond.notify_all();
}

void GlzDecoderWindow::add_decoded_image(GlzDecodedImage *image)
{
    Lock lock(_new_image_mutex);
    GLZ_ASSERT(_debug_calls, image->get_id() <= _tail_image_id);
    _images[calc_image_win_idx(image->get_id())] = image;
    _new_image_cond.notify_all();
}

/* Important observations:
   1) When an image is added, if it is not the first missing image, it is not removed
      immediately from the missing list (in order not to store another pointer to
      the missing list inside the window ,or otherwise, to go over the missing list
      and look for the image).
   2) images that weren't removed from the missing list in their addition time, will be removed when
      preliminary images will be added.
   3) The first item in the missing list is always really missing.
   4) The missing list is always ordered by image id (see add_pre_decoded_image)
*/
void GlzDecoderWindow::narrow_window(GlzDecodedImage *last_added)
{
    uint64_t new_head_image_id;

    GLZ_ASSERT(_debug_calls, !_missing_list.empty());

    if (_missing_list.front() != last_added->get_id()) {
        return;
    }

    _missing_list.pop_front();  // removing the last added image from the missing list

    /* maintaining the missing list: removing front images that already arrived */
    while (!_missing_list.empty()) {
        int front_win_idx = calc_image_win_idx(_missing_list.front());
        if (_images[front_win_idx] == NULL) { // still missing
            break;
        } else {
            _missing_list.pop_front();
        }
    }

    /* removing unnecessary image from the window's head*/
    if (_missing_list.empty()) {
        new_head_image_id = _images[
                calc_image_win_idx(_tail_image_id)]->get_window_head_id();
    } else {
        // there must be at least one image in the window since narrow_window is called
        // from post decode
        GLZ_ASSERT(_debug_calls, _images[calc_image_win_idx(_missing_list.front() - 1)]);
        new_head_image_id = _images[
                calc_image_win_idx(_missing_list.front() - 1)]->get_window_head_id();
    }

    remove_head(new_head_image_id);
}

inline void GlzDecoderWindow::remove_head(uint64_t new_head_image_id)
{
    GLZ_ASSERT(_debug_calls, _images[_head_idx]);

    int n_images_remove = (int)(new_head_image_id - _images[_head_idx]->get_id());

    if (!n_images_remove) {
        return;
    }

    for (int i = 0; i < n_images_remove; i++) {
        int index = (_head_idx + i) % _images_capacity;
        delete _images[index];
        _images[index] = NULL;
    }

    _n_images -= n_images_remove;
    _head_idx = (_head_idx + n_images_remove) % _images_capacity;

    _release_image_cond.notify_all();
}

/* NOTE: can only be used when the window is locked. (i.e, not inside get_ref_pixel) */
inline int GlzDecoderWindow::calc_image_win_idx(uint64_t image_id)
{
    return (int)((_head_idx + _n_images - 1 - (_tail_image_id - image_id)) % _images_capacity);
}

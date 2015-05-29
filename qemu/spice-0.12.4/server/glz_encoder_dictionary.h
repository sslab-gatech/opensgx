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

#ifndef _H_GLZ_ENCODER_DICTIONARY
#define _H_GLZ_ENCODER_DICTIONARY

#include <stdint.h>
#include "glz_encoder_config.h"

/*
    Interface for maintaining lz dictionary that is shared among several encoders.
    The interface for accessing the dictionary for encoding purposes is located in
    glz_encoder_dictionary_protected.h
*/

typedef void GlzEncDictContext;
typedef void GlzEncDictImageContext;

/* NOTE: DISPLAY_MIGRATE_DATA_VERSION should change in case GlzEncDictRestoreData changes*/
typedef struct GlzEncDictRestoreData {
    uint32_t size;
    uint32_t max_encoders;
    uint64_t last_image_id;
} GlzEncDictRestoreData;

/* size        : maximal number of pixels occupying the window
   max_encoders: maximal number of encoders that use the dictionary
   usr         : callbacks */
GlzEncDictContext *glz_enc_dictionary_create(uint32_t size, uint32_t max_encoders,
                                             GlzEncoderUsrContext *usr);

void glz_enc_dictionary_destroy(GlzEncDictContext *opaque_dict, GlzEncoderUsrContext *usr);

/* returns the window capacity in pixels */
uint32_t glz_enc_dictionary_get_size(GlzEncDictContext *);

/* returns the current state of the dictionary.
   NOTE - you should use it only when no encoder uses the dictionary. */
void glz_enc_dictionary_get_restore_data(GlzEncDictContext *opaque_dict,
                                         GlzEncDictRestoreData *out_data,
                                         GlzEncoderUsrContext *usr);

/* creates a dictionary and initialized it by use the given info */
GlzEncDictContext *glz_enc_dictionary_restore(GlzEncDictRestoreData *restore_data,
                                              GlzEncoderUsrContext *usr);

/*  NOTE - you should use this routine only when no encoder uses the dictionary. */
void glz_enc_dictionary_reset(GlzEncDictContext *opaque_dict, GlzEncoderUsrContext *usr);

/* image: the context returned by the encoder when the image was encoded.
   NOTE - you should use this routine only when no encoder uses the dictionary.*/
void glz_enc_dictionary_remove_image(GlzEncDictContext *opaque_dict,
                                     GlzEncDictImageContext *image, GlzEncoderUsrContext *usr);

#endif // _H_GLZ_ENCODER_DICTIONARY

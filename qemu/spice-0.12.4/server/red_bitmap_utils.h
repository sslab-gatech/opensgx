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

#ifdef RED_BITMAP_UTILS_RGB16
#define PIXEL rgb16_pixel_t
#define FNAME(name) name##_rgb16
#define GET_r(pix) (((pix) >> 10) & 0x1f)
#define GET_g(pix) (((pix) >> 5) & 0x1f)
#define GET_b(pix) ((pix) & 0x1f)
#endif

#if defined(RED_BITMAP_UTILS_RGB24) || defined(RED_BITMAP_UTILS_RGB32)
#define GET_r(pix) ((pix).r)
#define GET_g(pix) ((pix).g)
#define GET_b(pix) ((pix).b)
#endif

#ifdef RED_BITMAP_UTILS_RGB24
#define PIXEL rgb24_pixel_t
#define FNAME(name) name##_rgb24
#endif

#ifdef RED_BITMAP_UTILS_RGB32
#define PIXEL rgb32_pixel_t
#define FNAME(name) name##_rgb32
#endif


#define SAME_PIXEL_WEIGHT 0.5
#define NOT_CONTRAST_PIXELS_WEIGHT -0.25
#define CONTRAST_PIXELS_WEIGHT 1.0

#ifndef RED_BITMAP_UTILS_RGB16
#define CONTRAST_TH 60
#else
#define CONTRAST_TH 8
#endif


#define SAMPLE_JUMP 15
#define SAME_PIXEL(p1, p2) (GET_r(p1) == GET_r(p2) && GET_g(p1) == GET_g(p2) && \
                            GET_b(p1) == GET_b(p2))

static const double FNAME(PIX_PAIR_SCORE)[] = {
    SAME_PIXEL_WEIGHT,
    CONTRAST_PIXELS_WEIGHT,
    NOT_CONTRAST_PIXELS_WEIGHT,
};

// return 0 - equal, 1 - for contrast, 2 for no contrast (PIX_PAIR_SCORE is defined accordingly)
static inline int FNAME(pixelcmp)(PIXEL p1, PIXEL p2)
{
    int diff = ABS(GET_r(p1) - GET_r(p2));
    int equal;

    if (diff >= CONTRAST_TH) {
        return 1;
    }
    equal = !diff;

    diff = ABS(GET_g(p1) - GET_g(p2));

    if (diff >= CONTRAST_TH) {
        return 1;
    }

    equal = equal && !diff;

    diff = ABS(GET_b(p1) - GET_b(p2));
    if (diff >= CONTRAST_TH) {
        return 1;
    }
    equal = equal && !diff;

    if (equal) {
        return 0;
    } else {
        return 2;
    }
}

static inline double FNAME(pixels_square_score)(PIXEL *line1, PIXEL *line2)
{
    double ret = 0.0;
    int all_ident = TRUE;
    int cmp_res;
    cmp_res = FNAME(pixelcmp)(*line1, line1[1]);
    all_ident = all_ident && (!cmp_res);
    ret += FNAME(PIX_PAIR_SCORE)[cmp_res];
    cmp_res = FNAME(pixelcmp)(*line1, *line2);
    all_ident = all_ident && (!cmp_res);
    ret += FNAME(PIX_PAIR_SCORE)[cmp_res];
    cmp_res = FNAME(pixelcmp)(*line1, line2[1]);
    all_ident = all_ident && (!cmp_res);
    ret += FNAME(PIX_PAIR_SCORE)[cmp_res];

    // ignore squares where all pixels are identical
    if (all_ident) {
        ret -= (FNAME(PIX_PAIR_SCORE)[0]) * 3;
    }

    return ret;
}

static void FNAME(compute_lines_gradual_score)(PIXEL *lines, int width, int num_lines,
                                               double *o_samples_sum_score, int *o_num_samples)
{
    int jump = (SAMPLE_JUMP % width) ? SAMPLE_JUMP : SAMPLE_JUMP - 1;
    PIXEL *cur_pix = lines + width / 2;
    PIXEL *bottom_pix;
    PIXEL *last_line = lines + (num_lines - 1) * width;

    if ((width <= 1) || (num_lines <= 1)) {
        *o_num_samples = 1;
        *o_samples_sum_score = 1.0;
        return;
    }

    *o_samples_sum_score = 0;
    *o_num_samples = 0;

    while (cur_pix < last_line) {
        if ((cur_pix + 1 - lines) % width == 0) { // last pixel in the row
            cur_pix--; // jump is bigger than 1 so we will not enter endless loop
        }
        bottom_pix = cur_pix + width;
        (*o_samples_sum_score) += FNAME(pixels_square_score)(cur_pix, bottom_pix);
        (*o_num_samples)++;
        cur_pix += jump;
    }

    (*o_num_samples) *= 3;
}

#undef PIXEL
#undef FNAME
#undef SAME_PIXEL
#undef GET_r
#undef GET_g
#undef GET_b
#undef RED_BITMAP_UTILS_RGB16
#undef RED_BITMAP_UTILS_RGB24
#undef RED_BITMAP_UTILS_RGB32
#undef SAMPLE_JUMP
#undef CONTRAST_TH
#undef SAME_PIXEL_WEIGHT
#undef NOT_CONTRAST_PIXELS_WEIGHT

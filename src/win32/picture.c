/*****************************************************************************
 * common.c: Windows video output common code
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Martell Malone <martellmalone@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble: This file contains the functions related to the init of the vout
 *           structure, the common display code, the screensaver, but not the
 *           events and the Window Creation (events.c)
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "picture.h"

int CommonUpdatePicture(picture_t *picture, picture_t **fallback,
                        uint8_t *data, unsigned pitch)
{
    if (fallback) {
        if (*fallback == NULL) {
            *fallback = picture_NewFromFormat(&picture->format);
            if (*fallback == NULL)
                return VLC_EGENERIC;
        }
        for (int n = 0; n < picture->i_planes; n++) {
            const plane_t *src = &(*fallback)->p[n];
            plane_t       *dst = &picture->p[n];
            dst->p_pixels = src->p_pixels;
            dst->i_pitch  = src->i_pitch;
            dst->i_lines  = src->i_lines;
        }
        return VLC_SUCCESS;
    }
    /* fill in buffer info in first plane */
    picture->p->p_pixels = data;
    picture->p->i_pitch  = pitch;
    picture->p->i_lines  = picture->format.i_visible_height;

    /*  Fill chroma planes for biplanar YUV */
    if (picture->format.i_chroma == VLC_CODEC_NV12 ||
        picture->format.i_chroma == VLC_CODEC_NV21) {

        for (int n = 1; n < picture->i_planes; n++) {
            const plane_t *o = &picture->p[n-1];
            plane_t *p = &picture->p[n];

            p->p_pixels = o->p_pixels + o->i_lines * o->i_pitch;
            p->i_pitch  = pitch;
            p->i_lines  = picture->format.i_visible_height;
        }
        /* The dx/d3d buffer is always allocated as NV12 */
        if (vlc_fourcc_AreUVPlanesSwapped(picture->format.i_chroma, VLC_CODEC_NV12)) {
            /* TODO : Swap NV21 UV planes to match NV12 */
            return VLC_EGENERIC;
        }
    }

    /*  Fill chroma planes for planar YUV */
    if (picture->format.i_chroma == VLC_CODEC_I420 ||
        picture->format.i_chroma == VLC_CODEC_J420 ||
        picture->format.i_chroma == VLC_CODEC_YV12) {

        for (int n = 1; n < picture->i_planes; n++) {
            const plane_t *o = &picture->p[n-1];
            plane_t *p = &picture->p[n];

            p->p_pixels = o->p_pixels + o->i_lines * o->i_pitch;
            p->i_pitch  = pitch / 2;
            p->i_lines  = picture->format.i_visible_height / 2;
        }
        /* The dx/d3d buffer is always allocated as YV12 */
        if (vlc_fourcc_AreUVPlanesSwapped(picture->format.i_chroma, VLC_CODEC_YV12)) {
            uint8_t *p_tmp = picture->p[1].p_pixels;
            picture->p[1].p_pixels = picture->p[2].p_pixels;
            picture->p[2].p_pixels = p_tmp;
        }
    }
    return VLC_SUCCESS;
}

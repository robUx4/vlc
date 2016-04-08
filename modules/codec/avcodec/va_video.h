/*****************************************************************************
 * avcodec.h: decoder and encoder using libavcodec
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef VLC_CODEC_AVCODEC_VIDEO_H
#define VLC_CODEC_AVCODEC_VIDEO_H

#include "va.h"

int lavc_GetVideoFormat(decoder_t *dec, video_format_t *restrict fmt,
                               AVCodecContext *ctx, enum AVPixelFormat pix_fmt,
                               enum AVPixelFormat sw_pix_fmt, vlc_va_t *p_va);
int lavc_UpdateVideoFormat(decoder_t *dec, AVCodecContext *ctx,
                                         enum AVPixelFormat fmt,
                                         enum AVPixelFormat swfmt,
                                         vlc_va_t *p_va);

/**
 * Determines the VLC video chroma value for a pair of hardware acceleration
 * PixelFormat and software PixelFormat.
 * @param hwfmt the hardware acceleration pixel format
 * @param swfmt the software pixel format
 * @return a VLC chroma value, or 0 on error.
 */
vlc_fourcc_t vlc_va_GetChroma(enum PixelFormat hwfmt, enum PixelFormat swfmt);

#endif /* VLC_CODEC_AVCODEC_VIDEO_H */

/*****************************************************************************
 * video.c: video decoder using the libavcodec library
 *****************************************************************************
 * Copyright (C) 1999-2001 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

#ifndef AVCODEC_VIDEO_H_
#define AVCODEC_VIDEO_H_

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_avcodec.h>
#include <vlc_cpu.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>

#include "avcodec.h"
#include "va.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    AVCODEC_COMMON_MEMBERS

    /* Video decoder specific part */
    mtime_t i_pts;

    AVFrame          *p_ff_pic;

    /* for frame skipping algo */
    bool b_hurry_up;
    enum AVDiscard i_skip_frame;

    /* how many decoded frames are late */
    int     i_late_frames;
    mtime_t i_late_frames_start;

    /* for direct rendering */
    bool b_direct_rendering;
    int  i_direct_rendering_used;

    bool b_has_b_frames;

    /* Hack to force display of still pictures */
    bool b_first_frame;


    /* */
    bool palette_sent;

    /* */
    bool b_flush;

    /* VA API */
    vlc_va_t *p_va;

    vlc_sem_t sem_mt;
};

#endif /* AVCODEC_VIDEO_H_ */

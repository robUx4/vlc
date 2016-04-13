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
#include <vlc_atomic.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>

#include "avcodec.h"
#include "va.h"
#include "va_video.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    AVCODEC_COMMON_MEMBERS

    /* Video decoder specific part */
    mtime_t i_pts;

    /* for frame skipping algo */
    bool b_hurry_up;
    enum AVDiscard i_skip_frame;

    /* how many decoded frames are late */
    int     i_late_frames;
    mtime_t i_late_frames_start;

    /* for direct rendering */
    bool        b_direct_rendering;
    atomic_bool b_dr_failure;

    /* Hack to force display of still pictures */
    bool b_first_frame;


    /* */
    bool palette_sent;

    /* */
    bool b_flush;

    /* VA API */
    vlc_va_t *p_va;
    enum PixelFormat pix_fmt;
    int profile;
    int level;

    vlc_sem_t sem_mt;
};

/*****************************************************************************
 * Local Functions
 *****************************************************************************/
int lavc_GetVideoFormat(decoder_t *dec, video_format_t *restrict fmt,
                               AVCodecContext *ctx, enum AVPixelFormat pix_fmt,
                               enum AVPixelFormat sw_pix_fmt, vlc_va_t *p_va)
{
    int width = ctx->coded_width;
    int height = ctx->coded_height;

    video_format_Init(fmt, 0);

    if (pix_fmt == sw_pix_fmt)
    {   /* software decoding */
        int aligns[AV_NUM_DATA_POINTERS];

        if (GetVlcChroma(fmt, pix_fmt))
            return -1;

        avcodec_align_dimensions2(ctx, &width, &height, aligns);
    }
    else /* hardware decoding */
    {
        if ( p_va != NULL )
        {
            p_va->setup( p_va, fmt );
        }
        else
        {
            fmt->i_chroma = vlc_va_GetChroma(pix_fmt, sw_pix_fmt);
        }
    }

    if( width == 0 || height == 0 || width > 8192 || height > 8192 )
    {
        msg_Err(dec, "Invalid frame size %dx%d.", width, height);
        return -1; /* invalid display size */
    }

    fmt->i_width = width;
    fmt->i_height = height;
    fmt->i_visible_width = ctx->width;
    fmt->i_visible_height = ctx->height;

    /* If an aspect-ratio was specified in the input format then force it */
    if (dec->fmt_in.video.i_sar_num > 0 && dec->fmt_in.video.i_sar_den > 0)
    {
        fmt->i_sar_num = dec->fmt_in.video.i_sar_num;
        fmt->i_sar_den = dec->fmt_in.video.i_sar_den;
    }
    else
    {
        fmt->i_sar_num = ctx->sample_aspect_ratio.num;
        fmt->i_sar_den = ctx->sample_aspect_ratio.den;

        if (fmt->i_sar_num == 0 || fmt->i_sar_den == 0)
            fmt->i_sar_num = fmt->i_sar_den = 1;
    }

    if (dec->fmt_in.video.i_frame_rate > 0
     && dec->fmt_in.video.i_frame_rate_base > 0)
    {
        fmt->i_frame_rate = dec->fmt_in.video.i_frame_rate;
        fmt->i_frame_rate_base = dec->fmt_in.video.i_frame_rate_base;
    }
#if LIBAVCODEC_VERSION_CHECK( 56, 5, 0, 7, 100 )
    else if (ctx->framerate.num > 0 && ctx->framerate.den > 0)
    {
        fmt->i_frame_rate = ctx->framerate.num;
        fmt->i_frame_rate_base = ctx->framerate.den;
# if LIBAVCODEC_VERSION_MICRO <  100
        // for some reason libav don't thinkg framerate presents actually same thing as in ffmpeg
        fmt->i_frame_rate_base *= __MAX(ctx->ticks_per_frame, 1);
# endif
    }
#endif
    else if (ctx->time_base.num > 0 && ctx->time_base.den > 0)
    {
        fmt->i_frame_rate = ctx->time_base.den;
        fmt->i_frame_rate_base = ctx->time_base.num
                                 * __MAX(ctx->ticks_per_frame, 1);
    }
    return 0;
}

int lavc_UpdateVideoFormat(decoder_t *dec, AVCodecContext *ctx,
                                         enum AVPixelFormat fmt,
                                         enum AVPixelFormat swfmt,
                                         vlc_va_t *p_va)
{
    video_format_t fmt_out;
    int val;

    val = lavc_GetVideoFormat(dec, &fmt_out, ctx, fmt, swfmt, p_va);
    if (val)
        return val;

    es_format_Clean(&dec->fmt_out);
    es_format_Init(&dec->fmt_out, VIDEO_ES, fmt_out.i_chroma);
    dec->fmt_out.video = fmt_out;
    dec->fmt_out.video.orientation = dec->fmt_in.video.orientation;;
    return decoder_UpdateVideoFormat(dec);
}

vlc_fourcc_t vlc_va_GetChroma(enum PixelFormat hwfmt, enum PixelFormat swfmt)
{
    /* NOTE: At the time of writing this comment, the return value was only
     * used to probe support as decoder output. So incorrect values were not
     * fatal, especially not if a software format. */
    switch (hwfmt)
    {
        case AV_PIX_FMT_VAAPI_VLD:
            return VLC_CODEC_YV12;

        case AV_PIX_FMT_DXVA2_VLD:
            return VLC_CODEC_D3D9_OPAQUE;

#if LIBAVUTIL_VERSION_CHECK(54, 13, 1, 24, 100)
        case AV_PIX_FMT_D3D11VA_VLD:
            return VLC_CODEC_D3D11_OPAQUE;
#endif
#if (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(53, 14, 0))
        case AV_PIX_FMT_VDA:
            return VLC_CODEC_I420;
#endif
#if (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 4, 0))
        case AV_PIX_FMT_VDPAU:
            switch (swfmt)
            {
                case AV_PIX_FMT_YUVJ444P:
                case AV_PIX_FMT_YUV444P:
                    return VLC_CODEC_VDPAU_VIDEO_444;
                case AV_PIX_FMT_YUVJ422P:
                case AV_PIX_FMT_YUV422P:
                    return VLC_CODEC_VDPAU_VIDEO_422;
                case AV_PIX_FMT_YUVJ420P:
                case AV_PIX_FMT_YUV420P:
                    return VLC_CODEC_VDPAU_VIDEO_420;
                default:
                    return 0;
            }
            break;
#endif
        default:
            return 0;
    }
}

/*****************************************************************************
 * va.c: hardware acceleration plugins for avcodec
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * Copyright (C) 2012-2013 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_fourcc.h>
#include <vlc_codec.h>
#include <vlc_es.h>
#include <libavutil/pixfmt.h>
#include <libavcodec/avcodec.h>
#include "va.h"

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

static int vlc_va_Start(void *func, va_list ap)
{
    vlc_va_t *va = va_arg(ap, vlc_va_t *);
    AVCodecContext *ctx = va_arg(ap, AVCodecContext *);
    enum PixelFormat pix_fmt = va_arg(ap, enum PixelFormat);
    const es_format_t *fmt = va_arg(ap, const es_format_t *);
    decoder_t *p_dec = va_arg(ap, decoder_t *);
    vout_display_t *vout = va_arg(ap, vout_display_t *);
    int (*open)(vlc_va_t *, AVCodecContext *, enum PixelFormat,
                const es_format_t *, decoder_t *, vout_display_t *) = func;

    return open(va, ctx, pix_fmt, fmt, p_dec, vout);
}

static void vlc_va_Stop(void *func, va_list ap)
{
    vlc_va_t *va = va_arg(ap, vlc_va_t *);
    AVCodecContext *ctx = va_arg(ap, AVCodecContext *);
    void (*close)(vlc_va_t *, AVCodecContext *) = func;

    close(va, ctx);
}

static void GetOutputFormat(vlc_va_t *va, video_format_t *p_fmt_out)
{
    video_format_SetChroma( p_fmt_out, vlc_va_GetChroma( va->hwfmt ,0 ), NULL, 0 );
}

vlc_va_t *vlc_va_New(decoder_t *p_dec, AVCodecContext *avctx,
                     enum PixelFormat pix_fmt, const es_format_t *fmt, vout_display_t *vout)
{
    vlc_va_t *va = vlc_object_create(VLC_OBJECT(p_dec), sizeof (*va));
    if (unlikely(va == NULL))
        return NULL;
    va->hwfmt = pix_fmt;
    va->get_output = GetOutputFormat;

    va->module = vlc_module_load(va, "hw decoder", "$avcodec-hw", true,
                                 vlc_va_Start, va, avctx, pix_fmt, fmt, p_dec, vout);
    if (va->module == NULL)
    {
        vlc_object_release(va);
        va = NULL;
    }
    return va;
}

void vlc_va_Delete(vlc_va_t *va, AVCodecContext *avctx)
{
    vlc_module_unload(va->module, vlc_va_Stop, va, avctx);
    vlc_object_release(va);
}

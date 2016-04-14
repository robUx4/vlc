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
#include <libavutil/pixfmt.h>
#include <libavcodec/avcodec.h>
#include "va.h"
#include "va_video.h"

static int vlc_va_Start(void *func, va_list ap)
{
    vlc_va_t *va = va_arg(ap, vlc_va_t *);
    AVCodecContext *ctx = va_arg(ap, AVCodecContext *);
    enum PixelFormat pix_fmt = va_arg(ap, enum PixelFormat);
    const es_format_t *fmt = va_arg(ap, const es_format_t *);
    decoder_t *p_sys = va_arg(ap, decoder_t *);
    int (*open)(vlc_va_t *, AVCodecContext *, enum PixelFormat,
                const es_format_t *, decoder_t *) = func;

    return open(va, ctx, pix_fmt, fmt, p_sys);
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
                     enum PixelFormat pix_fmt, const es_format_t *fmt)
{
    vlc_va_t *va = vlc_object_create(VLC_OBJECT(p_dec), sizeof (*va));
    if (unlikely(va == NULL))
        return NULL;
    va->hwfmt = pix_fmt;
    va->get_output = GetOutputFormat;

    /* get a test picture from the vout */
    //picture_t *test_pic = decoder_GetPicture(p_dec);
    //assert(!test_pic /* || test_pic->format.i_chroma == p_dec->fmt_out.video.i_chroma */);

    //picture_sys_t *p_sys = test_pic ? test_pic->p_sys : NULL;

    va->module = vlc_module_load(va, "hw decoder", "$avcodec-hw", true,
                                 vlc_va_Start, va, avctx, pix_fmt, fmt, p_dec);
    if (va->module == NULL)
    {
        vlc_object_release(va);
        va = NULL;
    }
    //if (test_pic)
    //    picture_Release(test_pic);
    return va;
}

void vlc_va_Delete(vlc_va_t *va, AVCodecContext *avctx)
{
    vlc_module_unload(va->module, vlc_va_Stop, va, avctx);
    vlc_object_release(va);
}

/*****************************************************************************
 * directx_va.c: DirectX Generic Video Acceleration helpers
 *****************************************************************************
 * Copyright (C) 2009 Geoffroy Couprie
 * Copyright (C) 2009 Laurent Aimar
 * Copyright (C) 2015 Steve Lhomme
 * $Id$
 *
 * Authors: Geoffroy Couprie <geal@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Steve Lhomme <robux4@gmail.com>
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
# include "config.h"
#endif

# if _WIN32_WINNT < 0x600
/* d3d11 needs Vista support */
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x600
# endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_fourcc.h>
#include <vlc_plugin.h>
#include <vlc_codecs.h>

#include <libavcodec/avcodec.h>
#define COBJMACROS

#include "directx_va.h"

#include "avcodec.h"
#include "va.h"
#include "../../packetizer/h264_nal.h"

const int PROF_MPEG2_SIMPLE[2] = { FF_PROFILE_MPEG2_SIMPLE, 0 };
const int PROF_MPEG2_MAIN[3]   = { FF_PROFILE_MPEG2_SIMPLE,
                                  FF_PROFILE_MPEG2_MAIN, 0 };
const int PROF_H264_HIGH[4]    = { FF_PROFILE_H264_CONSTRAINED_BASELINE,
                                  FF_PROFILE_H264_MAIN,
                                  FF_PROFILE_H264_HIGH, 0 };
const int PROF_HEVC_MAIN[2]    = { FF_PROFILE_HEVC_MAIN, 0 };
const int PROF_HEVC_MAIN10[3]  = { FF_PROFILE_HEVC_MAIN,
                                  FF_PROFILE_HEVC_MAIN_10, 0 };


static void DestroyVideoDecoder(vlc_va_t *, directx_sys_t *);
static void DestroyVideoService(vlc_va_t *, directx_sys_t *);
static void DestroyDeviceManager(vlc_va_t *, directx_sys_t *);
static void DestroyDevice(vlc_va_t *, directx_sys_t *);

const directx_va_mode_t *directx_va_FindMode(const GUID *guid, const directx_va_mode_t dxva_modes[])
{
    for (unsigned i = 0; dxva_modes[i].name; i++) {
        if (IsEqualGUID(dxva_modes[i].guid, guid))
            return &dxva_modes[i];
    }
    return NULL;
}

/* */
int directx_va_Setup(vlc_va_t *va, directx_sys_t *dx_sys, AVCodecContext *avctx, vlc_fourcc_t *chroma)
{
    int surface_alignment = 16;
    int surface_count = 4;

    if (dx_sys->width == avctx->coded_width && dx_sys->height == avctx->coded_height
     && dx_sys->decoder != NULL)
        goto ok;

    /* */
    DestroyVideoDecoder(va, dx_sys);

    avctx->hwaccel_context = NULL;
    *chroma = 0;
    if (avctx->coded_width <= 0 || avctx->coded_height <= 0)
        return VLC_EGENERIC;

    /* */
    msg_Dbg(va, "directx_va_Setup id %d %dx%d", dx_sys->codec_id, avctx->coded_width, avctx->coded_height);

    switch ( dx_sys->codec_id )
    {
    case AV_CODEC_ID_MPEG2VIDEO:
        /* decoding MPEG-2 requires additional alignment on some Intel GPUs,
           but it causes issues for H.264 on certain AMD GPUs..... */
        surface_alignment = 32;
        surface_count += 2;
        break;
    case AV_CODEC_ID_HEVC:
        /* the HEVC DXVA2 spec asks for 128 pixel aligned surfaces to ensure
           all coding features have enough room to work with */
        surface_alignment = 128;
        surface_count += 16;
        break;
    case AV_CODEC_ID_H264:
        surface_count += 16;
        break;
    default:
        surface_count += 2;
    }

    if ( avctx->active_thread_type & FF_THREAD_FRAME )
        surface_count += dx_sys->thread_count;

    if (surface_count > MAX_SURFACE_COUNT)
        return VLC_EGENERIC;

    dx_sys->surface_count = surface_count;

#define ALIGN(x, y) (((x) + ((y) - 1)) & ~((y) - 1))
    dx_sys->width  = avctx->coded_width;
    dx_sys->height = avctx->coded_height;
    dx_sys->surface_width  = ALIGN(dx_sys->width, surface_alignment);
    dx_sys->surface_height = ALIGN(dx_sys->height, surface_alignment);

    /* FIXME transmit a video_format_t by VaSetup directly */
    video_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.i_width = dx_sys->width;
    fmt.i_height = dx_sys->height;
    fmt.i_frame_rate = avctx->framerate.num;
    fmt.i_frame_rate_base = avctx->framerate.den;

    if (dx_sys->pf_create_decoder_surfaces(va, dx_sys->codec_id, &fmt, avctx->active_thread_type & FF_THREAD_FRAME))
        return VLC_EGENERIC;

    for (int i = 0; i < dx_sys->surface_count; i++) {
        vlc_va_surface_t *surface = &dx_sys->surface[i];
        surface->refcount = 0;
        surface->order = 0;
        surface->p_lock = &dx_sys->surface_lock;
    }

    dx_sys->pf_setup_avcodec_ctx(va);

ok:
    return VLC_SUCCESS;
}

void DestroyVideoDecoder(vlc_va_t *va, directx_sys_t *dx_sys)
{
    dx_sys->pf_destroy_surfaces(va);

    for (int i = 0; i < dx_sys->surface_count; i++)
        IUnknown_Release( dx_sys->hw_surface[i] );

    if (dx_sys->decoder)
        IUnknown_Release( dx_sys->decoder );

    dx_sys->decoder = NULL;
    dx_sys->surface_count = 0;
}

/* FIXME it is nearly common with VAAPI */
int directx_va_Get(vlc_va_t *va, directx_sys_t *dx_sys, picture_t *pic, uint8_t **data)
{
    /* Check the device */
    if (dx_sys->pf_check_device(va)!=VLC_SUCCESS)
        return VLC_EGENERIC;

    vlc_mutex_lock( &dx_sys->surface_lock );

    /* Grab an unused surface, in case none are, try the oldest
     * XXX using the oldest is a workaround in case a problem happens with libavcodec */
    int i, old;
    for (i = 0, old = 0; i < dx_sys->surface_count; i++) {
        vlc_va_surface_t *surface = &dx_sys->surface[i];

        if (!surface->refcount)
            break;

        if (surface->order < dx_sys->surface[old].order)
            old = i;
    }
    if (i >= dx_sys->surface_count)
        i = old;

    vlc_va_surface_t *surface = &dx_sys->surface[i];

    surface->refcount = 1;
    surface->order = dx_sys->surface_order++;
    *data = (void *)dx_sys->hw_surface[i];
    pic->context = surface;

    vlc_mutex_unlock( &dx_sys->surface_lock );

    return VLC_SUCCESS;
}

void directx_va_Release(void *opaque, uint8_t *data)
{
    picture_t *pic = opaque;
    vlc_va_surface_t *surface = pic->context;
    vlc_mutex_lock( surface->p_lock );

    surface->refcount--;
    pic->context = NULL;
    picture_Release(pic);
    (void) data;

    vlc_mutex_unlock( surface->p_lock );
}

void directx_va_Close(vlc_va_t *va, directx_sys_t *dx_sys)
{
    DestroyVideoDecoder(va, dx_sys);
    DestroyVideoService(va, dx_sys);
    DestroyDeviceManager(va, dx_sys);
    DestroyDevice(va, dx_sys);

    if (dx_sys->hdecoder_dll)
        FreeLibrary(dx_sys->hdecoder_dll);
#if DEBUG_LEAK
    dx_sys->hdecoder_dll = NULL;
#endif

    vlc_mutex_destroy( &dx_sys->surface_lock );
}

int directx_va_Open(vlc_va_t *va, directx_sys_t *dx_sys,
                    AVCodecContext *ctx, const es_format_t *fmt)
{
    // TODO va->sys = sys;
    dx_sys->codec_id = ctx->codec_id;

    vlc_mutex_init( &dx_sys->surface_lock );

    /* Load dll*/
    dx_sys->hdecoder_dll = LoadLibrary(dx_sys->psz_decoder_dll);
    if (!dx_sys->hdecoder_dll) {
        msg_Warn(va, "cannot load DirectX decoder DLL");
        goto error;
    }
    msg_Dbg(va, "DLLs loaded");

    if (dx_sys->d3ddev) {
        msg_Dbg(va, "Reusing DirectX device");
    } else {
        /* */
        if (dx_sys->pf_create_device(va)) {
            msg_Err(va, "Failed to create DirectX device");
            goto error;
        }
        msg_Dbg(va, "CreateDevice succeed");
    }

    if (dx_sys->pf_create_device_manager(va)) {
        msg_Err(va, "D3dCreateDeviceManager failed");
        goto error;
    }

    if (dx_sys->pf_create_video_service(va)) {
        msg_Err(va, "DxCreateVideoService failed");
        goto error;
    }

    /* */
    if (dx_sys->pf_find_service_conversion(va, &dx_sys->input, fmt)) {
        msg_Err(va, "DxFindVideoServiceConversion failed");
        goto error;
    }

    dx_sys->thread_count = ctx->thread_count;

    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}

bool profile_supported(const directx_va_mode_t *mode, const es_format_t *fmt)
{
    bool is_supported = mode->p_profiles == NULL || !mode->p_profiles[0];
    if (!is_supported)
    {
        int profile = fmt->i_profile;
        if (mode->codec == AV_CODEC_ID_H264)
        {
            size_t h264_profile;
            if ( h264_get_profile_level(fmt, &h264_profile, NULL, NULL) )
                profile = h264_profile;
        }

        if (profile <= 0)
            is_supported = true;
        else for (const int *p_profile = &mode->p_profiles[0]; *p_profile; ++p_profile)
        {
            if (*p_profile == profile)
            {
                is_supported = true;
                break;
            }
        }
    }
    return is_supported;
}

void DestroyVideoService(vlc_va_t *va, directx_sys_t *dx_sys)
{
    dx_sys->pf_destroy_video_service(va);
    if (dx_sys->d3ddec)
        IUnknown_Release(dx_sys->d3ddec);
#if DEBUG_LEAK
    dx_sys->d3ddec = NULL;
#endif
}

void DestroyDeviceManager(vlc_va_t *va, directx_sys_t *dx_sys)
{
    dx_sys->pf_destroy_device_manager(va);
}

void DestroyDevice(vlc_va_t *va, directx_sys_t *dx_sys)
{
    dx_sys->pf_destroy_device(va);
    if (dx_sys->d3ddev)
        IUnknown_Release( dx_sys->d3ddev );
#if DEBUG_LEAK
    dx_sys->d3ddev = NULL;
#endif
}

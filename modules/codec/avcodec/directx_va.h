/*****************************************************************************
 * directx_va.h: DirectX Generic Video Acceleration helpers
 *****************************************************************************
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

#ifndef AVCODEC_DIRECTX_VA_H
#define AVCODEC_DIRECTX_VA_H

#define DEBUG_LEAK 1

# if _WIN32_WINNT < 0x600
/* d3d11 needs Vista support */
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x600
# endif

#include <vlc_common.h>

#include <libavcodec/avcodec.h>
#include "avcodec.h"
#include "va.h"

#include <unknwn.h>

const int PROF_MPEG2_SIMPLE[2];
const int PROF_MPEG2_MAIN[3];
const int PROF_H264_HIGH[4];
const int PROF_HEVC_MAIN[2];
const int PROF_HEVC_MAIN10[3];

typedef struct {
    const char   *name;
    const GUID   *guid;
    int          codec;
    const int    *p_profiles; // NULL or ends with 0
} directx_va_mode_t;

/* */
typedef struct {
    IUnknown           *d3d;
    int                refcount;
    unsigned int       order;
    vlc_mutex_t        *p_lock;
} vlc_va_surface_t;

#define MAX_SURFACE_COUNT (64)
typedef struct
{
    int          codec_id;
    int          width;
    int          height;

    vlc_mutex_t     surface_lock;

    /* DLL */
    HINSTANCE             hdecoder_dll;
    const TCHAR           *psz_decoder_dll;

    /* Direct3D */
    IUnknown              *d3ddev;

    /* Video service */
    GUID                   input;
    IUnknown               *d3ddec;

    /* Video decoder */
    IUnknown               *decoder;

    /* */
    int          surface_count;
    int          surface_order;
    int          surface_width;
    int          surface_height;

    int          thread_count;

    vlc_va_surface_t surface[MAX_SURFACE_COUNT];
    void*            hw_surface[MAX_SURFACE_COUNT];

    int (*pf_create_decoder)(vlc_va_t *, int codec_id, const video_format_t *fmt, bool b_threading);
    void (*pf_destroy_decoder)(vlc_va_t *);
    void (*pf_setup_avcodec_ctx)(vlc_va_t *);

    int (*pf_check_device)(vlc_va_t *);

    int (*pf_create_device)(vlc_va_t *);
    void (*pf_destroy_device)(vlc_va_t *);

    int (*pf_create_device_manager)(vlc_va_t *);
    void (*pf_destroy_device_manager)(vlc_va_t *);

    int (*pf_create_video_service)(vlc_va_t *);
    void (*pf_destroy_video_service)(vlc_va_t *);

    int (*pf_find_service_conversion)(vlc_va_t *, GUID *input, const es_format_t *fmt);

} directx_sys_t;

int directx_va_Open(vlc_va_t *, directx_sys_t *, AVCodecContext *ctx, const es_format_t *fmt);
void directx_va_Close(vlc_va_t *, directx_sys_t *);
int directx_va_Setup(vlc_va_t *, directx_sys_t *, AVCodecContext *avctx, vlc_fourcc_t *chroma);

int directx_va_CreateVideoDecoder(vlc_va_t *, int codec_id, const video_format_t *fmt, bool b_threading);

int directx_va_Get(vlc_va_t *, directx_sys_t *, picture_t *pic, uint8_t **data);
void directx_va_Release(void *opaque, uint8_t *data);

const directx_va_mode_t *directx_va_FindMode(const GUID *guid, const directx_va_mode_t dxva_modes[]);
bool profile_supported(const directx_va_mode_t *mode, const es_format_t *fmt);

#endif /* AVCODEC_DIRECTX_VA_H */

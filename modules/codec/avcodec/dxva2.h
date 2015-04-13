/*****************************************************************************
 * dxva2.c: Video Acceleration helpers
 *****************************************************************************
 * Copyright (C) 2009 Geoffroy Couprie
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Geoffroy Couprie <geal@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef AVCODEC_DXVA2_H_
#define AVCODEC_DXVA2_H_

#define DEBUG_SURFACE 0

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

# if _WIN32_WINNT < 0x600
/* dxva2 needs Vista support */
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x600
# endif

#include <vlc_common.h>
#include <vlc_picture.h>

#include "va.h"

#include <d3d9.h>

struct picture_pool_setup_sys_t {
    vlc_va_sys_t           *p_va_sys;
    /* shared objects between the decoder and the d3d9 vout */
    LPDIRECT3D9            d3dobj;
    LPDIRECT3DDEVICE9      d3ddev;
    D3DPRESENT_PARAMETERS  d3dpp;
};

<<<<<<< HEAD
struct picture_sys_t
{
    LPDIRECT3DSURFACE9 surface;
    LPDIRECT3DDEVICE9  d3ddev; // TODO not needed anymore ?
=======
    /* Device manager */
    UINT                     token;
    IDirect3DDeviceManager9  *devmng;
    HANDLE                   device;

    /* Video service */
    IDirectXVideoDecoderService  *vs;
    GUID                         input;
    const d3d_format_t           *p_render;

    /* Video decoder */
    DXVA2_ConfigPictureDecode    cfg;
    IDirectXVideoDecoder         *decoder;

    /* Option conversion */
    D3DFORMAT                    output;

    /* */
    struct dxva_context hw;

    /* */
    unsigned     surface_count;
    unsigned     surface_order;
    int          surface_width;
    int          surface_height;
    vlc_fourcc_t surface_chroma;
>>>>>>> 563754c... DIRECT_DXVA is always set

    picture_t          *fallback;

    /* DXVA stuff */
    int                refcount;
    unsigned int       order;
    unsigned int       index;
    vlc_mutex_t        *p_lock;
#if DEBUG_SURFACE
    vlc_va_t           *p_va;
#endif
};

#endif /* AVCODEC_DXVA2_H_ */

/*****************************************************************************
 * direct3d9.c: Windows Direct3D9 video output module
 *****************************************************************************
 * Copyright (C) 2006-2014 VLC authors and VideoLAN
 *$Id$
 *
 * Authors: Martell Malone <martellmalone@gmail.com>,
 *          Damien Fouilleul <damienf@videolan.org>,
 *          Sasha Koruga <skoruga@gmail.com>,
 *          Felix Abecassis <felix.abecassis@gmail.com>
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
 * Preamble:
 *
 * This plugin will use YUV surface if supported, using YUV will result in
 * the best video quality (hardware filering when rescaling the picture)
 * and the fastest display as it requires less processing.
 *
 * If YUV overlay is not supported this plugin will use RGB offscreen video
 * surfaces that will be blitted onto the primary surface (display) to
 * effectively display the pictures.
 *
 *****************************************************************************/

#ifndef MSW_DIRECT3D9_H_
#define MSW_DIRECT3D9_H_

#define DEBUG_SURFACE 1
<<<<<<< HEAD
#define DIRECT_DXVA 0
=======

#define LOCK_SURFACE 1

>>>>>>> 563754c... DIRECT_DXVA is always set

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#include <windows.h>
#include <d3d9.h>

#include "common.h"

typedef struct vlc_va_t vlc_va_t;

typedef struct
{
    const char   *name;
    D3DFORMAT    format;    /* D3D format */
    vlc_fourcc_t fourcc;    /* VLC fourcc */
    uint32_t     rmask;
    uint32_t     gmask;
    uint32_t     bmask;
} d3d_format_t;

struct picture_sys_t
{
    LPDIRECT3DSURFACE9 surface;
    picture_t          *fallback;

    /* DXVA stuff */
    int                refcount;
    unsigned int       order;
    unsigned int       index;
    vlc_mutex_t        *p_lock;
    const d3d_format_t *p_ouput;
#if DEBUG_SURFACE
    vlc_va_t          *va;
#endif
};

#endif /* MSW_DIRECT3D9_H_ */

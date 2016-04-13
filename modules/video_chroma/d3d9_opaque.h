/*****************************************************************************
 * d3d9_opaque.h : Direct3D9 shared picture format structures
 *****************************************************************************
 * Copyright (C) 2016 VLC authors, VideoLAN and VideoLabs
 * $Id$
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
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

#ifndef VLC_D3D9_OPAQUE_H
#define VLC_D3D9_OPAQUE_H

#include <vlc_common.h>

#include <windows.h>
#include <d3d9.h>

typedef struct
{
    IDirect3DDevice9      *d3ddevice;
} pool_factory_d3d9;

typedef struct
{
    /* shared between all pool (factory) handling VLC_CODEC_D3D9_OPAQUE */
    LPDIRECT3D9           d3dobj;
    LPDIRECT3DDEVICE9     d3ddevice;

    /* each pool may have a different texture format */
    D3DFORMAT             format;    /* D3D format */

    /* DLL */
    HINSTANCE             hd3d9_dll;

} picture_pool_d3d9;

struct picture_sys_t
{
    LPDIRECT3DSURFACE9 surface;
    picture_t          *fallback;
};

struct sub_chroma
{
    D3DFORMAT             format;
};

int D3D9CreateSurfaceContext( vlc_object_t *, picture_pool_d3d9 * );
void D3D9DestroySurfaceContext( void * );
void D3D9SurfaceContextAddRef( void * );
void D3D9SurfaceContextDelRef( void * );
picture_pool_t* D3D9CreateSurfacePool( vlc_object_t *, pool_picture_factory *,
                                       const video_format_t *, unsigned );
#endif /* VLC_D3D9_OPAQUE_H */

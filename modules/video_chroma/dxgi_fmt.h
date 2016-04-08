/*****************************************************************************
 * d3d11_surface.c : DXGI helper calls
 *****************************************************************************
 * Copyright © 2015 VLC authors, VideoLAN and VideoLabs
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

#ifndef VLC_VIDEOCHROMA_DXGI_FMT_H_
#define VLC_VIDEOCHROMA_DXGI_FMT_H_

#include <dxgiformat.h>

#include <vlc_common.h>
#include <vlc_fourcc.h>

#include <d3d11.h>

typedef struct picture_pool_t picture_pool_t;

typedef struct
{
    const char   *name;
    DXGI_FORMAT  formatTexture;
    vlc_fourcc_t fourcc;
    DXGI_FORMAT  formatY;
    DXGI_FORMAT  formatUV;
} d3d_format_t;

extern const char *DxgiFormatToStr(DXGI_FORMAT format);
extern const d3d_format_t *GetRenderFormatList(void);

typedef struct
{
    /* shared between all pool (factory) handling VLC_CODEC_D3D11_OPAQUE */
    vlc_object_t         *p_obj;
    ID3D11Device         *d3ddevice;
    ID3D11DeviceContext  *d3dcontext;

    /* each pool may have a different texture format */
    DXGI_FORMAT          textureFormat;

    HINSTANCE            hdecoder_dll;
#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    HINSTANCE            dxgidebug_dll;
#endif
} picture_pool_d3d11;

int D3D11CreateSurfaceContext( vlc_object_t *, picture_pool_d3d11 * );
void D3D11DestroySurfaceContext( void * );
void D3D11SurfaceContextAddRef( void * );
void D3D11SurfaceContextDelRef( void * );
picture_pool_t* D3D11CreateSurfacePool( pool_picture_factory *, const video_format_t *, unsigned );

#endif /* include-guard */

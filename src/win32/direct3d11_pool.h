/*****************************************************************************
 * direct3d9_pool.c: Windows Direct3D9 picture pool creation
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 *$Id$
 *
 * Authors: Steve Lhomme <robux4@gmail.com>,
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

#ifndef WIN32_DIRECT3D11_POOL_H_
#define WIN32_DIRECT3D11_POOL_H_

#include <windows.h>
#include <d3d11.h>

#include <vlc_common.h>
#include <vlc_picture_pool.h>

#define VA_DXVA2_MAX_SURFACE_COUNT (64)

typedef struct
{
    ID3D11Texture2D           *pTexture;
    ID3D11ShaderResourceView  *d3dresViewY;
    ID3D11ShaderResourceView  *d3dresViewUV;
} d3d11_texture_t;

picture_pool_t *AllocPoolD3D11( vlc_object_t *obj, const video_format_t *p_fmt, unsigned pool_size );
picture_pool_t *AllocPoolD3D11Ex(vlc_object_t *obj, ID3D11Device *d3ddev,
                                 const video_format_t *fmt, DXGI_FORMAT output,
                                 unsigned pool_size);

#endif /* WIN32_DIRECT3D11_POOL_H_ */

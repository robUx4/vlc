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

picture_pool_t *AllocPoolD3D11( vlc_object_t *obj, const video_format_t *, unsigned pool_size );
picture_pool_t *AllocPoolD3D11Ex(vlc_object_t *, ID3D11Device *, ID3D11DeviceContext *,
                                 const video_format_t *, DXGI_FORMAT, unsigned pool_size);

#endif /* WIN32_DIRECT3D11_POOL_H_ */

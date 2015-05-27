/*****************************************************************************
 * d3d11_surface.c : D3D11 GPU surface conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>

#include "copy.h"

static int  OpenConverter( vlc_object_t * );
static void CloseConverter( vlc_object_t * );

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Conversions from D3D11VA to NV12,I420L,YUY2") )
    set_capability( "video filter2", 10 )
    set_callbacks( OpenConverter, CloseConverter )
vlc_module_end ()

#include <windows.h>
#define COBJMACROS
#include <d3d11.h>

struct picture_sys_t
{
    ID3D11VideoDecoderOutputView  *surface;
    ID3D11DeviceContext           *context;
};

static bool GetLock(filter_t *p_filter, ID3D11VideoDecoderOutputView *d3d,
                    ID3D11DeviceContext *pDeviceContext,
                    D3D11_MAPPED_SUBRESOURCE *p_lock,
                    ID3D11Resource **pResource)
{
    ID3D11VideoDecoderOutputView_GetResource(d3d, pResource);

    /* */
    if (FAILED(ID3D11DeviceContext_Map(pDeviceContext, *pResource, 0, D3D11_MAP_READ, 0, p_lock))) {
        msg_Err(p_filter, "Failed to map surface");
        return false;
    }

    return true;
}

static void D3D11_I420L(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    copy_cache_t *p_copy_cache = (copy_cache_t*) p_filter->p_sys;

    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE lock;
    ID3D11Resource *pResource;
    if (!GetLock(p_filter, src->p_sys->surface, src->p_sys->context, &lock, &pResource))
    {
        ID3D11Resource_Release(pResource);
        return;
    }

    ID3D11Texture2D_GetDesc((ID3D11Texture2D*) pResource, &desc);

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    if (desc.Format == DXGI_FORMAT_P010) {
        bool imc3 = false;
        size_t chroma_pitch = imc3 ? lock.RowPitch : (lock.RowPitch / 2);

        size_t pitch[3] = {
            lock.RowPitch,
            chroma_pitch,
            chroma_pitch,
        };

        uint8_t *plane[3] = {
            (uint8_t*)lock.pData,
            (uint8_t*)lock.pData + pitch[0] * src->format.i_height,
            (uint8_t*)lock.pData + pitch[0] * src->format.i_height
                                 + pitch[1] * src->format.i_height / 2,
        };

        if (imc3) {
            uint8_t *V = plane[1];
            plane[1] = plane[2];
            plane[2] = V;
        }
        CopyFromYv12(dst, plane, pitch, src->format.i_width,
                     src->format.i_visible_height, p_copy_cache);
    } else if (desc.Format == DXGI_FORMAT_NV12) {
        uint8_t *plane[2] = {
            lock.pData,
            (uint8_t*)lock.pData + lock.RowPitch * src->format.i_visible_height
        };
        size_t  pitch[2] = {
            lock.RowPitch,
            lock.RowPitch,
        };
        CopyFromNv12(dst, plane, pitch, src->format.i_width,
                     src->format.i_visible_height, p_copy_cache);
    } else {
        msg_Err(p_filter, "Unsupported D3D11VA conversion from 0x%08X to YV12", desc.Format);
    }

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    /* */
    ID3D11DeviceContext_Unmap(src->p_sys->context, pResource, 0);
    ID3D11Resource_Release(pResource);
}

static void D3D11_YUY2(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    copy_cache_t *p_copy_cache = (copy_cache_t*) p_filter->p_sys;

    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE lock;
    ID3D11Resource *pResource;
    if (!GetLock(p_filter, src->p_sys->surface, src->p_sys->context, &lock, &pResource))
    {
        ID3D11Resource_Release(pResource);
        return;
    }

    ID3D11Texture2D_GetDesc((ID3D11Texture2D*) pResource, &desc);

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    if (desc.Format == DXGI_FORMAT_YUY2) {
        bool imc3 = false;
        size_t chroma_pitch = imc3 ? lock.RowPitch : (lock.RowPitch / 2);

        size_t pitch[3] = {
            lock.RowPitch,
            chroma_pitch,
            chroma_pitch,
        };

        uint8_t *plane[3] = {
            (uint8_t*)lock.pData,
            (uint8_t*)lock.pData + pitch[0] * src->format.i_height,
            (uint8_t*)lock.pData + pitch[0] * src->format.i_height
                                 + pitch[1] * src->format.i_height / 2,
        };

        if (imc3) {
            uint8_t *V = plane[1];
            plane[1] = plane[2];
            plane[2] = V;
        }
        CopyFromYv12(dst, plane, pitch, src->format.i_width,
                     src->format.i_visible_height, p_copy_cache);
    } else if (desc.Format == DXGI_FORMAT_NV12) {
        uint8_t *plane[2] = {
            lock.pData,
            (uint8_t*)lock.pData + lock.RowPitch * src->format.i_visible_height
        };
        size_t  pitch[2] = {
            lock.RowPitch,
            lock.RowPitch,
        };
        CopyFromNv12(dst, plane, pitch, src->format.i_width,
                     src->format.i_visible_height, p_copy_cache);
    } else {
        msg_Err(p_filter, "Unsupported D3D11VA conversion from 0x%08X to YV12", desc.Format);
    }

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    /* */
    ID3D11DeviceContext_Unmap(src->p_sys->context, pResource, 0);
    ID3D11Resource_Release(pResource);
}

static void D3D11_NV12(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    copy_cache_t *p_copy_cache = (copy_cache_t*) p_filter->p_sys;

    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE lock;
    ID3D11Resource *pResource;
    if (!GetLock(p_filter, src->p_sys->surface, src->p_sys->context, &lock, &pResource))
    {
        ID3D11Resource_Release(pResource);
        return;
    }

    ID3D11Texture2D_GetDesc((ID3D11Texture2D*) pResource, &desc);

    if (desc.Format == DXGI_FORMAT_NV12) {
        uint8_t *plane[2] = {
            lock.pData,
            (uint8_t*)lock.pData + lock.RowPitch * src->format.i_visible_height
        };
        size_t  pitch[2] = {
            lock.RowPitch,
            lock.RowPitch,
        };
        CopyFromNv12ToNv12(dst, plane, pitch, src->format.i_width,
                           src->format.i_visible_height, p_copy_cache);
    } else {
        msg_Err(p_filter, "Unsupported D3D11VA conversion from 0x%08X to NV12", desc.Format);
    }

    /* */
    ID3D11DeviceContext_Unmap(src->p_sys->context, pResource, 0);
    ID3D11Resource_Release(pResource);
}

VIDEO_FILTER_WRAPPER (D3D11_I420L)
VIDEO_FILTER_WRAPPER (D3D11_NV12)
VIDEO_FILTER_WRAPPER (D3D11_YUY2)

static int OpenConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    if ( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D11_OPAQUE )
        return VLC_EGENERIC;

    if ( p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
         || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width )
        return VLC_EGENERIC;

    switch( p_filter->fmt_out.video.i_chroma ) {
    case VLC_CODEC_I420:
    case VLC_CODEC_YV12:
        p_filter->pf_video_filter = D3D11_YUY2_Filter;
        break;
    case VLC_CODEC_NV12:
        p_filter->pf_video_filter = D3D11_NV12_Filter;
        break;
    default:
        return VLC_EGENERIC;
    }

    copy_cache_t *p_copy_cache = calloc(1, sizeof(*p_copy_cache));
    if (!p_copy_cache)
         return VLC_ENOMEM;
    CopyInitCache(p_copy_cache, p_filter->fmt_in.video.i_width );
    p_filter->p_sys = (filter_sys_t*) p_copy_cache;

    return VLC_SUCCESS;
}

static void CloseConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    copy_cache_t *p_copy_cache = (copy_cache_t*) p_filter->p_sys;
    CopyCleanCache(p_copy_cache);
    free( p_copy_cache );
    p_filter->p_sys = NULL;
}

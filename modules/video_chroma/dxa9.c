/*****************************************************************************
 * dxa9.c : DXVA2 GPU surface conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2015 VLC authors, VideoLAN and VideoLabs
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
    set_description( N_("Conversions from DxVA2 to YUV") )
    set_capability( "video filter2", 10 )
    set_callbacks( OpenConverter, CloseConverter )
vlc_module_end ()

#include <windows.h>
#include <d3d9.h>

typedef struct
{
    /* shared between all pool (factory) handling VLC_CODEC_D3D9_OPAQUE */
    LPDIRECT3D9           d3dobj;
    LPDIRECT3DDEVICE9     d3ddev;

    /* each pool may have a different texture format */
    D3DFORMAT             format;    /* D3D format */

    /* DLL */
    HINSTANCE             hd3d9_dll;

} picture_pool_d3d9;

struct picture_sys_t
{
    LPDIRECT3DSURFACE9 surface;
};

struct filter_sys_t {
    copy_cache_t       cache;
    picture_pool_d3d9  pool_factory;
};

int D3D9CreateSurfaceContext( vlc_object_t *, picture_pool_d3d9 * );
void D3D9DestroySurfaceContext( void * );
void D3D9SurfaceContextAddRef( void * );
void D3D9SurfaceContextDelRef( void * );
picture_pool_t* D3D9CreateSurfacePool( vlc_object_t *, pool_picture_factory *,
                                       const video_format_t *, unsigned );


int D3D9CreateSurfaceContext( vlc_object_t *p_obj, picture_pool_d3d9 *sys )
{
    /* Load dll*/
    sys->hd3d9_dll = LoadLibrary(TEXT("D3D9.DLL"));
    if (!sys->hd3d9_dll) {
        msg_Warn(p_obj, "cannot load d3d9.dll");
        goto error;
    }
    /* */
    LPDIRECT3D9 (WINAPI *Create9)(UINT SDKVersion);
    Create9 = (void *)GetProcAddress(sys->hd3d9_dll, "Direct3DCreate9");
    if (!Create9) {
        msg_Err(p_obj, "Cannot locate reference to Direct3DCreate9 ABI in DLL");
        goto error;
    }

    /* */
    LPDIRECT3D9 d3dobj;
    d3dobj = Create9(D3D_SDK_VERSION);
    if (!d3dobj) {
        msg_Err(p_obj, "Direct3DCreate9 failed");
        goto error;
    }
    sys->d3dobj = d3dobj;

    /* */
    UINT AdapterToUse = D3DADAPTER_DEFAULT;
    D3DDEVTYPE DeviceType = D3DDEVTYPE_HAL;

#ifndef NDEBUG
    // Look for 'NVIDIA PerfHUD' adapter
    // If it is present, override default settings
    for (UINT Adapter=0; Adapter< IDirect3D9_GetAdapterCount(d3dobj); ++Adapter) {
        D3DADAPTER_IDENTIFIER9 Identifier;
        HRESULT Res = IDirect3D9_GetAdapterIdentifier(d3dobj,Adapter,0,&Identifier);
        if (SUCCEEDED(Res) && strstr(Identifier.Description,"PerfHUD") != 0) {
            AdapterToUse = Adapter;
            DeviceType = D3DDEVTYPE_REF;
            break;
        }
    }
#endif

    /* */
    D3DADAPTER_IDENTIFIER9 d3dai;
    if (FAILED(IDirect3D9_GetAdapterIdentifier(d3dobj, AdapterToUse, 0, &d3dai))) {
        msg_Warn( p_obj, "IDirect3D9_GetAdapterIdentifier failed");
    } else {
        msg_Dbg( p_obj, "Direct3d9 Device: %s %lu %lu %lu", d3dai.Description,
                d3dai.VendorId, d3dai.DeviceId, d3dai.Revision );
    }

    /* */
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp.Windowed               = TRUE;
    d3dpp.hDeviceWindow          = NULL;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp.BackBufferCount        = 0;                  /* FIXME what to put here */
    d3dpp.BackBufferFormat       = D3DFMT_X8R8G8B8;    /* FIXME what to put here */
    d3dpp.BackBufferWidth        = 0;
    d3dpp.BackBufferHeight       = 0;
    d3dpp.EnableAutoDepthStencil = FALSE;

    /* Direct3D needs a HWND to create a device, even without using ::Present
    this HWND is used to alert Direct3D when there's a change of focus window.
    For now, use GetDesktopWindow, as it looks harmless */
    LPDIRECT3DDEVICE9 d3ddev;
    if (FAILED(IDirect3D9_CreateDevice(d3dobj, AdapterToUse,
                                       DeviceType, GetDesktopWindow(),
                                       D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                                       D3DCREATE_MULTITHREADED,
                                       &d3dpp, &d3ddev))) {
        msg_Err( p_obj, "IDirect3D9_CreateDevice failed");
        return VLC_EGENERIC;
    }
    sys->d3ddev = d3ddev;

    return VLC_SUCCESS;

error:
    if ( sys->d3dobj )
        IDirect3D9_Release( sys->d3dobj );
    if (sys->hd3d9_dll)
        FreeLibrary( sys->hd3d9_dll );
    return VLC_EGENERIC;
}

void D3D9DestroySurfaceContext( void *p_opaque )
{
    picture_pool_d3d9 *sys = p_opaque;

    if ( sys->d3ddev )
        IDirect3DDevice9_Release( sys->d3ddev );
    if ( sys->d3dobj )
        IDirect3D9_Release( sys->d3dobj );
    if (sys->hd3d9_dll)
        FreeLibrary( sys->hd3d9_dll );
}

static void DestroyPoolPictureD3D9(picture_t *picture)
{
    picture_sys_t *p_sys = (picture_sys_t*) picture->p_sys;

    if (p_sys->surface)
        IDirect3DSurface9_Release(p_sys->surface);

    free(p_sys);
    free(picture);
}

picture_pool_t* D3D9CreateSurfacePool( vlc_object_t *p_obj, pool_picture_factory *p_factory,
                                       const video_format_t *fmt, unsigned count )
{
    picture_pool_d3d9  *sys = p_factory->p_opaque;

    picture_t**       pictures = NULL;
    unsigned          picture_count = 0;

    pictures = calloc(count, sizeof(*pictures));
    if (!pictures)
        goto error;
    for (picture_count = 0; picture_count < count; ++picture_count)
    {
        picture_sys_t *picsys = malloc(sizeof(*picsys));
        if (unlikely(picsys == NULL))
            goto error;

        HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(sys->d3ddev,
                                                          fmt->i_width,
                                                          fmt->i_height,
                                                          sys->format, // TODO MAKEFOURCC('N','V','1','2'),
                                                          D3DPOOL_DEFAULT,
                                                          &picsys->surface,
                                                          NULL);
        if (FAILED(hr)) {
           msg_Err( p_obj, "Failed to allocate surface %d (hr=0x%0lx)", picture_count, hr);
           goto error;
        }

        picture_resource_t resource = {
            .p_sys = picsys,
            .pf_destroy = DestroyPoolPictureD3D9,
        };

        picture_t *picture = picture_NewFromResource( fmt, &resource );
        if (unlikely(picture == NULL)) {
            free(picsys);
            goto error;
        }

        pictures[picture_count] = picture;
    }

    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = count;
    pool_cfg.picture       = pictures;

    picture_pool_t *pool = picture_pool_NewExtended( &pool_cfg );

error:
    if (pool == NULL && pictures) {
        for (unsigned i=0;i<picture_count; ++i)
            DestroyPoolPictureD3D9( pictures[i] );
    }
    free(pictures);
    return pool;
}

void D3D9SurfaceContextAddRef( void *p_opaque )
{
    picture_pool_d3d9 *sys = p_opaque;

    IDirect3DDevice9_AddRef( sys->d3ddev );
    IDirect3D9_AddRef( sys->d3dobj );
}

void D3D9SurfaceContextDelRef( void *p_opaque )
{
    picture_pool_d3d9 *sys = p_opaque;

    IDirect3DDevice9_Release( sys->d3ddev );
    IDirect3D9_Release( sys->d3dobj );
}

static bool GetLock(filter_t *p_filter, LPDIRECT3DSURFACE9 d3d,
                    D3DLOCKED_RECT *p_lock, D3DSURFACE_DESC *p_desc)
{
    if (FAILED( IDirect3DSurface9_GetDesc(d3d, p_desc)))
        return false;

    /* */
    if (FAILED(IDirect3DSurface9_LockRect(d3d, p_lock, NULL, D3DLOCK_READONLY))) {
        msg_Err(p_filter, "Failed to lock surface");
        return false;
    }

    return true;
}

static void DXA9_YV12(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    copy_cache_t *p_copy_cache = (copy_cache_t*) p_filter->p_sys;

    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lock;
    if (!GetLock(p_filter, src->p_sys->surface, &lock, &desc))
        return;

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    if (desc.Format == MAKEFOURCC('Y','V','1','2') ||
        desc.Format == MAKEFOURCC('I','M','C','3')) {
        bool imc3 = desc.Format == MAKEFOURCC('I','M','C','3');
        size_t chroma_pitch = imc3 ? lock.Pitch : (lock.Pitch / 2);

        size_t pitch[3] = {
            lock.Pitch,
            chroma_pitch,
            chroma_pitch,
        };

        uint8_t *plane[3] = {
            (uint8_t*)lock.pBits,
            (uint8_t*)lock.pBits + pitch[0] * src->format.i_height,
            (uint8_t*)lock.pBits + pitch[0] * src->format.i_height
                                 + pitch[1] * src->format.i_height / 2,
        };

        if (imc3) {
            uint8_t *V = plane[1];
            plane[1] = plane[2];
            plane[2] = V;
        }
        CopyFromYv12(dst, plane, pitch, src->format.i_width,
                     src->format.i_height, p_copy_cache);
    } else if (desc.Format == MAKEFOURCC('N','V','1','2')) {
        uint8_t *plane[2] = {
            lock.pBits,
            (uint8_t*)lock.pBits + lock.Pitch * src->format.i_height
        };
        size_t  pitch[2] = {
            lock.Pitch,
            lock.Pitch,
        };
        CopyFromNv12(dst, plane, pitch, src->format.i_width,
                     src->format.i_height, p_copy_cache);
    } else {
        msg_Err(p_filter, "Unsupported DXA9 conversion from 0x%08X to YV12", desc.Format);
    }

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    /* */
    IDirect3DSurface9_UnlockRect(src->p_sys->surface);
}

static void DXA9_NV12(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    copy_cache_t *p_copy_cache = (copy_cache_t*) p_filter->p_sys;

    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lock;
    if (!GetLock(p_filter, src->p_sys->surface, &lock, &desc))
        return;

    if (desc.Format == MAKEFOURCC('N','V','1','2')) {
        uint8_t *plane[2] = {
            lock.pBits,
            (uint8_t*)lock.pBits + lock.Pitch * src->format.i_height
        };
        size_t  pitch[2] = {
            lock.Pitch,
            lock.Pitch,
        };
        CopyFromNv12ToNv12(dst, plane, pitch, src->format.i_width,
                           src->format.i_height, p_copy_cache);
    } else {
        msg_Err(p_filter, "Unsupported DXA9 conversion from 0x%08X to NV12", desc.Format);
    }

    /* */
    IDirect3DSurface9_UnlockRect(src->p_sys->surface);
}

VIDEO_FILTER_WRAPPER (DXA9_YV12)
VIDEO_FILTER_WRAPPER (DXA9_NV12)

static int OpenConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    if ( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE )
        return VLC_EGENERIC;

    if ( p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
         || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width )
        return VLC_EGENERIC;

    switch( p_filter->fmt_out.video.i_chroma ) {
    case VLC_CODEC_I420:
    case VLC_CODEC_YV12:
        p_filter->pf_video_filter = DXA9_YV12_Filter;
        break;
    case VLC_CODEC_NV12:
        p_filter->pf_video_filter = DXA9_NV12_Filter;
        break;
    default:
        return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = calloc(1, sizeof(filter_sys_t));
    if (!p_sys)
         return VLC_ENOMEM;
    p_filter->pool_factory.p_opaque = &p_sys->pool_factory;

    pool_picture_factory *p_pool_factory = pool_HandlerGetFactory( p_filter->p_pool_handler,
                                                                   p_filter->fmt_in.video.i_chroma,
                                                                   false );
    if ( p_pool_factory != NULL )
    {
        D3D9SurfaceContextAddRef( p_filter->pool_factory.p_opaque );
        p_filter->pool_factory.pf_destructor = D3D9SurfaceContextDelRef;
    }
    else
    {
        int err = D3D9CreateSurfaceContext( VLC_OBJECT(p_filter), &p_sys->pool_factory );
        if ( err != VLC_SUCCESS )
        {
            free( p_sys );
            return err;
        }
        p_filter->pool_factory.pf_destructor = D3D9DestroySurfaceContext;
    }
    p_filter->pool_factory.pf_create_pool = D3D9CreateSurfacePool;

    CopyInitCache(&p_sys->cache, p_filter->fmt_in.video.i_width );
    p_filter->p_sys = p_sys;

    return VLC_SUCCESS;
}

static void CloseConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = p_filter->p_sys;
    CopyCleanCache( &p_sys->cache );
    free( p_sys );
    p_filter->p_sys = NULL;
}

/*****************************************************************************
 * d3d9_opaque.c : Direct3D9 shared picture initialization
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_picture.h>
#include <vlc_picture_pool.h>

#include "d3d9_opaque.h"

int D3D9CreateChromaContext( vlc_object_t *p_obj, vlc_chroma_context *sys )
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
    sys->d3ddevice = d3ddev;

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
    vlc_chroma_context *sys = p_opaque;

    if ( sys->d3ddevice )
        IDirect3DDevice9_Release( sys->d3ddevice );
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

void D3D9SurfaceContextAddRef( void *p_opaque )
{
    vlc_chroma_context *sys = p_opaque;

    IDirect3DDevice9_AddRef( sys->d3ddevice );
    IDirect3D9_AddRef( sys->d3dobj );
}

void D3D9SurfaceContextDelRef( void *p_opaque )
{
    vlc_chroma_context *sys = p_opaque;

    IDirect3DDevice9_Release( sys->d3ddevice );
    IDirect3D9_Release( sys->d3dobj );
}

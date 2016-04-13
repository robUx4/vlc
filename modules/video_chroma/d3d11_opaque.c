/*****************************************************************************
 * d3d11_opaque.c : D3D11 shared picture initialization
 *****************************************************************************
 * Copyright © 2016 VLC authors, VideoLAN and VideoLabs
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_es.h>
#include <vlc_picture.h>
#include <vlc_picture_pool.h>

#define COBJMACROS
#define INITGUID
#include "dxgi_fmt.h"
#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
# include <dxgidebug.h>
#endif

#include "d3d11_opaque.h"

int D3D11CreateChromaContext( vlc_object_t *p_obj, vlc_chroma_context *sys )
{
    HRESULT hr;
    memset( sys, 0, sizeof(*sys) );

#if VLC_WINSTORE_APP
    sys->hdecoder_dll = NULL;
    sys->d3ddevice = var_InheritInteger( p_obj, "winrt-d3ddevice");
    if ( sys->d3ddevice == NULL )
        return VLC_EGENERIC;
    sys->d3dcontext = var_InheritInteger( p_obj, "winrt-d3dcontext");
    if ( sys->d3dcontext == NULL )
        return VLC_EGENERIC;
#else /* VLC_WINSTORE_APP */

    sys->hdecoder_dll = LoadLibrary( TEXT("D3D11.DLL") );
    if (!sys->hdecoder_dll) {
        msg_Warn( p_obj, "cannot load DirectX decoder DLL" );
        goto error;
    }
    msg_Dbg( p_obj, "D3D11 DLLs loaded" );

    /* */
    PFN_D3D11_CREATE_DEVICE pf_CreateDevice;
    pf_CreateDevice = (void *)GetProcAddress(sys->hdecoder_dll, "D3D11CreateDevice");
    if (!pf_CreateDevice) {
        msg_Err( p_obj, "Cannot locate reference to D3D11CreateDevice ABI in DLL");
        goto error;
    }

    UINT creationFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#if !defined(NDEBUG) //&& defined(_MSC_VER)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    /* */
    hr = pf_CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                                 creationFlags, NULL, 0,
                                 D3D11_SDK_VERSION, &sys->d3ddevice, NULL, &sys->d3dcontext);
    if (FAILED(hr)) {
        msg_Err( p_obj, "D3D11CreateDevice failed. (hr=0x%lX)", hr);
        goto error;
    }

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    HRESULT (WINAPI  * pf_DXGIGetDebugInterface)(const GUID *riid, void **ppDebug);
    sys->dxgidebug_dll = LoadLibrary(TEXT("DXGIDEBUG.DLL"));
    if ( sys->dxgidebug_dll) {
        pf_DXGIGetDebugInterface = (void *)GetProcAddress( sys->dxgidebug_dll, "DXGIGetDebugInterface");
        if (pf_DXGIGetDebugInterface) {
            IDXGIDebug *pDXGIDebug = NULL;
            hr = pf_DXGIGetDebugInterface(&IID_IDXGIDebug, (void**)&pDXGIDebug);
            if (SUCCEEDED(hr) && pDXGIDebug) {
                hr = IDXGIDebug_ReportLiveObjects(pDXGIDebug, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
            }
        }
    }
#endif
#endif
    return VLC_SUCCESS;


error:
    D3D11DestroySurfaceContext( sys );
    return VLC_EGENERIC;
}

void D3D11DestroySurfaceContext( void *p_opaque )
{
    picture_pool_d3d11 *sys = p_opaque;

    if ( sys->d3dcontext )
        ID3D11DeviceContext_Release( sys->d3dcontext );
    if ( sys->d3ddevice )
        ID3D11Device_Release( sys->d3ddevice );
    if ( sys->hdecoder_dll )
        FreeLibrary( sys->hdecoder_dll );
#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    if (sys->dxgidebug_dll)
        FreeLibrary(sys->dxgidebug_dll);
#endif
}

void D3D11SurfaceContextAddRef( void *p_opaque )
{
    picture_pool_d3d11 *sys = p_opaque;

    IUnknown_AddRef( sys->d3ddevice );
    IUnknown_AddRef( sys->d3dcontext );
}

void D3D11SurfaceContextDelRef( void *p_opaque )
{
    picture_pool_d3d11 *sys = p_opaque;

    IUnknown_Release( sys->d3ddevice );
    IUnknown_Release( sys->d3dcontext );
}

static void DestroyDisplayPoolPicture(picture_t *picture)
{
    picture_sys_t *p_sys = (picture_sys_t*) picture->p_sys;

    if (p_sys->texture)
        ID3D11Texture2D_Release(p_sys->texture);

    free(p_sys);
    free(picture);
}

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "direct3d9_pool.h"

static void DestroyPicture(picture_t *picture);

struct picture_sys_t {
    LPDIRECT3DSURFACE9 surface;
    HINSTANCE          hd3d9_dll;
};

picture_pool_t *AllocPoolD3D9( vlc_object_t *va, const video_format_t *fmt, unsigned pool_size )
{
    HINSTANCE         hd3d9_dll = NULL;
    LPDIRECT3D9       d3dobj = NULL;
    LPDIRECT3DDEVICE9 d3ddev = NULL;

    if (fmt->i_chroma != VLC_CODEC_D3D9_OPAQUE)
        return picture_pool_NewFromFormat(fmt, pool_size);

    OSVERSIONINFO winVer;
    winVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if(GetVersionEx(&winVer) && winVer.dwMajorVersion < 6) {
        msg_Warn(va, "windows version not compatible with D3D9");
        goto error;
    }

    hd3d9_dll = LoadLibrary(TEXT("D3D9.DLL"));
    if (!hd3d9_dll) {
        msg_Warn(va, "cannot load d3d9.dll, aborting");
        goto error;
    }

    LPDIRECT3D9 (WINAPI *OurDirect3DCreate9)(UINT SDKVersion);
    OurDirect3DCreate9 = (void *)GetProcAddress(hd3d9_dll, "Direct3DCreate9");
    if (!OurDirect3DCreate9) {
        msg_Err(va, "Cannot locate reference to Direct3DCreate9 ABI in DLL");
        goto error;
    }

    /* Create the D3D object. */
    d3dobj = OurDirect3DCreate9(D3D_SDK_VERSION);
    if (!d3dobj) {
       msg_Err(va, "Could not create Direct3D9 instance.");
       goto error;
    }

    /*
    ** Get device capabilities
    */
    D3DCAPS9    d3dcaps;
    ZeroMemory(&d3dcaps, sizeof(d3dcaps));
    HRESULT hr = IDirect3D9_GetDeviceCaps(d3dobj, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dcaps);
    if (FAILED(hr)) {
       msg_Err(va, "Could not read adapter capabilities. (hr=0x%0lx)", hr);
       goto error;
    }

    /* TODO: need to test device capabilities and select the right render function */
    if (!(d3dcaps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES) ||
        !(d3dcaps.TextureFilterCaps & (D3DPTFILTERCAPS_MAGFLINEAR)) ||
        !(d3dcaps.TextureFilterCaps & (D3DPTFILTERCAPS_MINFLINEAR))) {
        msg_Err(va, "Device does not support stretching from textures.");
        goto error;
    }

    // Create the D3DDevice
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
    if (FAILED(IDirect3D9_GetAdapterIdentifier(d3dobj,AdapterToUse,0, &d3dai))) {
        msg_Warn(va, "IDirect3D9_GetAdapterIdentifier failed");
    } else {
        msg_Dbg(va, "Direct3d9 Device: %s %lu %lu %lu", d3dai.Description,
                d3dai.VendorId, d3dai.DeviceId, d3dai.Revision );
    }

    /*
    ** Get the current desktop display mode, so we can set up a back
    ** buffer of the same format
    */
    D3DDISPLAYMODE d3ddm;
    hr = IDirect3D9_GetAdapterDisplayMode(d3dobj, D3DADAPTER_DEFAULT, &d3ddm);
    if (FAILED(hr)) {
       msg_Err(va, "Could not read adapter display mode. (hr=0x%0lx)", hr);
       goto error;
    }

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp.Windowed               = TRUE;
    d3dpp.hDeviceWindow          = NULL;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp.BackBufferCount        = 0; /* FIXME may not be right */
    d3dpp.BackBufferFormat       = d3ddm.Format;
    d3dpp.BackBufferWidth        = __MAX((unsigned int)GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                          d3ddm.Width);
    d3dpp.BackBufferHeight       = __MAX((unsigned int)GetSystemMetrics(SM_CYVIRTUALSCREEN),
                                          d3ddm.Height);
    d3dpp.EnableAutoDepthStencil = FALSE;

    hr = IDirect3D9_CreateDevice(d3dobj, AdapterToUse,
                                         DeviceType, GetDesktopWindow(),
                                         D3DCREATE_SOFTWARE_VERTEXPROCESSING|
                                         D3DCREATE_MULTITHREADED,
                                         &d3dpp, &d3ddev);
    if (FAILED(hr)) {
       msg_Err(va, "Could not create the D3D9 device! (hr=0x%0lx)", hr);
       goto error;
    }

    picture_pool_t *result = AllocPoolD3D9Ex(va, d3ddev, fmt, pool_size);
    if (!result)
        goto error;

    FreeLibrary(hd3d9_dll);
    return result;

error:
    if (d3ddev)
        IDirect3DDevice9_Release(d3ddev);
    if (d3dobj)
        IDirect3D9_Release(d3dobj);
    if (hd3d9_dll)
        FreeLibrary(hd3d9_dll);
    return NULL;
}

picture_pool_t *AllocPoolD3D9Ex(vlc_object_t *va, LPDIRECT3DDEVICE9 d3ddev,
                                const video_format_t *fmt, unsigned pool_size)
{
    picture_t**       pictures = NULL;
    unsigned          picture_count = 0;

    pictures = calloc(pool_size, sizeof(*pictures));
    if (!pictures)
        goto error;
    for (picture_count = 0; picture_count < pool_size; ++picture_count)
    {
        picture_sys_t *picsys = malloc(sizeof(*picsys));
        if (unlikely(picsys == NULL))
            goto error;

        HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(d3ddev,
                                                          fmt->i_visible_width,
                                                          fmt->i_visible_height,
                                                          MAKEFOURCC('N','V','1','2'), /* FIXME d3ddm.Format, */
                                                          D3DPOOL_DEFAULT,
                                                          &picsys->surface,
                                                          NULL);
        if (FAILED(hr)) {
           msg_Err(va, "Failed to allocate surface %d (hr=0x%0lx)", picture_count, hr);
           goto error;
        }

        picture_resource_t resource = {
            .p_sys = picsys,
            .pf_destroy = DestroyPicture,
        };

        picture_t *picture = picture_NewFromResource(fmt, &resource);
        if (unlikely(picture == NULL)) {
            free(picsys);
            goto error;
        }

        pictures[picture_count] = picture;
        /* each picture_t holds a ref to the device and release it on Destroy */
        IDirect3DDevice9_AddRef(d3ddev);
        /* each picture_t holds a ref to the DLL */
        picsys->hd3d9_dll = LoadLibrary(TEXT("D3D9.DLL"));
    }

    /* release the system resources, they will be free'd with the pool */
    IDirect3DDevice9_Release(d3ddev);

    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = pool_size;
    pool_cfg.picture       = pictures;

    return picture_pool_NewExtended( &pool_cfg );

error:
    if (pictures) {
        for (unsigned i=0;i<picture_count; ++i)
            DestroyPicture(pictures[i]);
        free(pictures);
    }
    return NULL;
}

static void DestroyPicture(picture_t *picture)
{
    LPDIRECT3DDEVICE9 d3ddev;
    if (!FAILED(IDirect3DSurface9_GetDevice(picture->p_sys->surface, &d3ddev)))
        IDirect3DDevice9_Release(d3ddev);

    IDirect3DSurface9_Release(picture->p_sys->surface);

    FreeLibrary(picture->p_sys->hd3d9_dll);

    free(picture->p_sys);
}

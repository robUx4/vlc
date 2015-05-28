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

#include "picture.h"

#define COBJMACROS
#include "direct3d11_pool.h"

#include <initguid.h> /* must be last included to not redefine existing GUIDs */

#ifdef __MINGW32__
# include <_mingw.h>

# if defined(__MINGW64_VERSION_MAJOR)
#  include <dxva.h>
# endif

#endif /* __MINGW32__ */

DEFINE_GUID(IID_ID3D10Multithread,   0x9b7e4e00, 0x342c, 0x4106, 0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0);

static int Direct3D11MapTexture(picture_t *);
static void DestroyPicture(picture_t *);

struct picture_sys_t {
    d3d11_texture_t     texture;
    ID3D11Device        *device;
    ID3D11DeviceContext *context;
    HINSTANCE           hd3d11_dll;
};

typedef struct
{
    const char   *name;
    DXGI_FORMAT  formatTexture;
    vlc_fourcc_t fourcc;
    DXGI_FORMAT  formatY;
    DXGI_FORMAT  formatUV;
} d3d_format_t;

static const d3d_format_t d3d_formats[] = {
    { "I420",     DXGI_FORMAT_NV12,           VLC_CODEC_I420,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
    { "YV12",     DXGI_FORMAT_NV12,           VLC_CODEC_YV12,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
    { "NV12",     DXGI_FORMAT_NV12,           VLC_CODEC_NV12,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
    { "D3D11VA",  DXGI_FORMAT_NV12,           VLC_CODEC_D3D11_OPAQUE,  DXGI_FORMAT_R8_UNORM,      DXGI_FORMAT_R8G8_UNORM },
#ifdef BROKEN_PIXEL
    { "YUY2",     DXGI_FORMAT_YUY2,           VLC_CODEC_I422,     DXGI_FORMAT_R8G8B8A8_UNORM,     0 },
    { "AYUV",     DXGI_FORMAT_AYUV,           VLC_CODEC_YUVA,     DXGI_FORMAT_R8G8B8A8_UNORM,     0 },
    { "Y416",     DXGI_FORMAT_Y416,           VLC_CODEC_I444_16L, DXGI_FORMAT_R16G16B16A16_UINT,  0 },
#endif
#ifdef UNTESTED
    { "P010",     DXGI_FORMAT_P010,           VLC_CODEC_I420_10L, DXGI_FORMAT_R16_UNORM,          DXGI_FORMAT_R16_UNORM },
    { "Y210",     DXGI_FORMAT_Y210,           VLC_CODEC_I422_10L, DXGI_FORMAT_R16G16B16A16_UNORM, 0 },
    { "Y410",     DXGI_FORMAT_Y410,           VLC_CODEC_I444_10L, DXGI_FORMAT_R10G10B10A2_UNORM,  0 },
    { "NV11",     DXGI_FORMAT_NV11,           VLC_CODEC_I411,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
#endif
    { "R8G8B8A8", DXGI_FORMAT_R8G8B8A8_UNORM, VLC_CODEC_RGBA,     DXGI_FORMAT_R8G8B8A8_UNORM,     0 },
    { "B8G8R8A8", DXGI_FORMAT_B8G8R8A8_UNORM, VLC_CODEC_BGRA,     DXGI_FORMAT_B8G8R8A8_UNORM,     0 },
    { "R8G8B8X8", DXGI_FORMAT_B8G8R8X8_UNORM, VLC_CODEC_RGB32,    DXGI_FORMAT_B8G8R8X8_UNORM,     0 },
    { "B5G6R5",   DXGI_FORMAT_B5G6R5_UNORM,   VLC_CODEC_RGB16,    DXGI_FORMAT_B5G6R5_UNORM,       0 },

    { NULL, 0, 0, 0, 0}
};

picture_pool_t *AllocPoolD3D11( vlc_object_t *va, const video_format_t *fmt, unsigned pool_size )
{
    HINSTANCE         hd3d11_dll = NULL;
    ID3D11Device      *d3ddev = NULL;
    HRESULT           hr;

    if (fmt->i_chroma != VLC_CODEC_D3D11_OPAQUE)
        return picture_pool_NewFromFormat(fmt, pool_size);

    OSVERSIONINFO winVer;
    winVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if(GetVersionEx(&winVer) && winVer.dwMajorVersion < 6) {
        msg_Warn(va, "windows version not compatible with D3D11");
        goto error;
    }

    hd3d11_dll = LoadLibrary(TEXT("D3D11.DLL"));
    if (!hd3d11_dll) {
        msg_Warn(va, "cannot load d3d11.dll, aborting");
        goto error;
    }

    PFN_D3D11_CREATE_DEVICE pf_CreateDevice;
    pf_CreateDevice = (PFN_D3D11_CREATE_DEVICE) GetProcAddress(hd3d11_dll, "D3D11CreateDevice");
    if (!pf_CreateDevice) {
        msg_Err(va, "Cannot locate reference to D3D11CreateDevice ABI in DLL");
        goto error;
    }

    /* Create the D3D object. */
    UINT creationFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
# if !defined(NDEBUG) && defined(_MSC_VER)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
# endif

    /* */
    static const D3D_DRIVER_TYPE driverAttempts[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };

    ID3D11DeviceContext *d3dctx;
    for (UINT driver = 0; driver < ARRAYSIZE(driverAttempts); driver++) {
        hr = pf_CreateDevice(NULL, driverAttempts[driver], NULL,
                                 creationFlags, NULL, 0,
                                 D3D11_SDK_VERSION, &d3ddev, NULL, &d3dctx);
        if (SUCCEEDED(hr)) {
#ifndef NDEBUG
            msg_Dbg(va, "Created the D3D11 pool device 0x%p ctx 0x%p type %d.", d3ddev, d3dctx, driverAttempts[driver]);
#endif
            break;
        }
    }
    if (FAILED(hr)) {
        msg_Err(va, "D3D11CreateDevice failed. (hr=0x%lX)", hr);
        goto error;
    }
#if 0
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
#endif

    vlc_fourcc_t i_src_chroma;
    if (fmt->i_chroma == VLC_CODEC_D3D9_OPAQUE)
        i_src_chroma = VLC_CODEC_NV12; // favor NV12
    else
        i_src_chroma = fmt->i_chroma;

    d3d11_texture_cfg_t outputFormat = {
        .textureFormat =  DXGI_FORMAT_UNKNOWN,
    };

    // look for the request pixel format first
    UINT i_quadSupportFlags = D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    UINT i_formatSupport;
    for (unsigned i = 0; d3d_formats[i].name != 0; i++)
    {
        if( i_src_chroma == d3d_formats[i].fourcc)
        {
            if( SUCCEEDED( ID3D11Device_CheckFormatSupport(d3ddev,
                                                           d3d_formats[i].formatTexture,
                                                           &i_formatSupport)) &&
                    ( i_formatSupport & i_quadSupportFlags ) == i_quadSupportFlags)
            {
                msg_Dbg(va, "Using pixel format %s", d3d_formats[i].name );
                outputFormat.textureFormat      = d3d_formats[i].formatTexture;
                outputFormat.resourceFormatYRGB = d3d_formats[i].formatY;
                outputFormat.resourceFormatUV   = d3d_formats[i].formatUV;
                break;
            }
        }
    }

    // look for any pixel format that we can handle
    if ( outputFormat.textureFormat == DXGI_FORMAT_UNKNOWN )
    {
        for (unsigned i = 0; d3d_formats[i].name != 0; i++)
        {
            if( SUCCEEDED( ID3D11Device_CheckFormatSupport(d3ddev,
                                                           d3d_formats[i].formatTexture,
                                                           &i_formatSupport)) &&
                    ( i_formatSupport & i_quadSupportFlags ) == i_quadSupportFlags)
            {
                msg_Dbg(va, "Using pixel format %s", d3d_formats[i].name );
                outputFormat.textureFormat      = d3d_formats[i].formatTexture;
                outputFormat.resourceFormatYRGB = d3d_formats[i].formatY;
                outputFormat.resourceFormatUV   = d3d_formats[i].formatUV;
                break;
            }
        }
    }
    if ( outputFormat.textureFormat == DXGI_FORMAT_UNKNOWN )
    {
       msg_Err(va, "Could not get a suitable texture pixel format");
       goto error;
    }

    picture_pool_t *result = AllocPoolD3D11Ex(va, d3ddev, d3dctx, fmt, &outputFormat, pool_size);
    if (!result)
        goto error;

    FreeLibrary(hd3d11_dll);
    return result;

error:
    if (d3ddev)
        ID3D11Device_Release(d3ddev);
    if (hd3d11_dll)
        FreeLibrary(hd3d11_dll);
    return NULL;
}

picture_pool_t *AllocPoolD3D11Ex(vlc_object_t *va, ID3D11Device *d3ddev, ID3D11DeviceContext *d3dctx,
                                 const video_format_t *fmt, const d3d11_texture_cfg_t *cfg,
                                 unsigned pool_size)
{
    picture_t**       pictures = NULL;
    unsigned          picture_count = 0;
    HRESULT           hr;

    ID3D10Multithread *pMultithread;
    hr = ID3D11Device_QueryInterface( d3ddev, &IID_ID3D10Multithread, (void **)&pMultithread);
    if (SUCCEEDED(hr)) {
        ID3D10Multithread_SetMultithreadProtected(pMultithread, TRUE);
        ID3D10Multithread_Release(pMultithread);
    }

    pictures = calloc(pool_size, sizeof(*pictures));
    if (!pictures)
        goto error;

    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width = fmt->i_width;
    texDesc.Height = fmt->i_height;
    texDesc.MipLevels = 1;
    texDesc.Format = cfg->textureFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0; //D3D11_RESOURCE_MISC_SHARED;
    texDesc.ArraySize = 1;
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    unsigned surface_count;
    for (surface_count = 0; surface_count < pool_size; surface_count++) {
        picture_sys_t *picsys = calloc(1, sizeof(*picsys));
        if (unlikely(picsys == NULL))
            goto error;

        hr = ID3D11Device_CreateTexture2D( d3ddev, &texDesc, NULL, &picsys->texture.pTexture );
        if (FAILED(hr)) {
            msg_Err(va, "CreateTexture2D %d failed. (hr=0x%0lx)", pool_size, hr);
            goto error;
        }

        picsys->context = d3dctx;
        picsys->device = d3ddev;

        picture_resource_t resource = {
            .p_sys = picsys,
            .pf_destroy = DestroyPicture,
        };

        picture_t *picture = picture_NewFromResource(fmt, &resource);
        if (unlikely(picture == NULL)) {
            free(picsys);
            goto error;
        }

        pictures[surface_count] = picture;
        /* each picture_t holds a ref to the context and release it on Destroy */
        ID3D11DeviceContext_AddRef(picsys->context);
        /* each picture_t holds a ref to the DLL */
        picsys->hd3d11_dll = LoadLibrary(TEXT("D3D11.DLL"));
    }
    msg_Dbg(va, "ID3D11VideoDecoderOutputView succeed with %d surfaces (%dx%d)",
            pool_size, fmt->i_width, fmt->i_height);

    /* release the system resources, they will be free'd with the pool */
    //ID3D11Device_Release(d3ddev); /* TODO check this */

    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = pool_size;
    pool_cfg.picture       = pictures;
    pool_cfg.lock          = Direct3D11MapTexture; /* TODO we only need to map once */

    return picture_pool_NewExtended( &pool_cfg );

error:
    if (pictures) {
        for (unsigned i=0;i<picture_count; ++i)
            DestroyPicture(pictures[i]);
        free(pictures);
    }
    return NULL;
}

static int Direct3D11MapTexture(picture_t *picture)
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr;
    int res;
    hr = ID3D11DeviceContext_Map(picture->p_sys->context, (ID3D11Resource *)picture->p_sys->texture.pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if( FAILED(hr) )
        return VLC_EGENERIC;

    res = CommonUpdatePicture(picture, NULL, mappedResource.pData, mappedResource.RowPitch);
    ID3D11DeviceContext_Unmap(picture->p_sys->context,(ID3D11Resource *)picture->p_sys->texture.pTexture, 0);
    return res;
}

static void DestroyPicture(picture_t *picture)
{
    ID3D11DeviceContext_Release(picture->p_sys->context);

    D3D11TextureRelease(&picture->p_sys->texture);

    FreeLibrary(picture->p_sys->hd3d11_dll);

    free(picture->p_sys);
    free(picture);
}

void D3D11TextureRelease(d3d11_texture_t *p_texture)
{
    if (p_texture->pTexture)
        ID3D11Texture2D_Release(p_texture->pTexture);
    if (p_texture->d3dresViewY)
        ID3D11ShaderResourceView_Release(p_texture->d3dresViewY);
    if (p_texture->d3dresViewUV)
        ID3D11ShaderResourceView_Release(p_texture->d3dresViewUV);
}

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

    vlc_fourcc_t i_src_chroma = fmt->i_chroma;

    DXGI_FORMAT outputFormat =  DXGI_FORMAT_UNKNOWN;

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
                outputFormat = d3d_formats[i].formatTexture;
                break;
            }
        }
    }

    // look for any pixel format that we can handle
    if ( outputFormat == DXGI_FORMAT_UNKNOWN )
    {
        for (unsigned i = 0; d3d_formats[i].name != 0; i++)
        {
            if( SUCCEEDED( ID3D11Device_CheckFormatSupport(d3ddev,
                                                           d3d_formats[i].formatTexture,
                                                           &i_formatSupport)) &&
                    ( i_formatSupport & i_quadSupportFlags ) == i_quadSupportFlags)
            {
                msg_Dbg(va, "Using pixel format %s", d3d_formats[i].name );
                outputFormat = d3d_formats[i].formatTexture;
                break;
            }
        }
    }
    if ( outputFormat == DXGI_FORMAT_UNKNOWN )
    {
       msg_Err(va, "Could not get a suitable texture pixel format");
       goto error;
    }

    picture_pool_t *result = AllocPoolD3D11Ex(va, d3ddev, d3dctx, fmt, outputFormat, pool_size);
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
                                 const video_format_t *fmt, DXGI_FORMAT output_format,
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
    texDesc.Format = output_format;
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

        hr = ID3D11Device_CreateTexture2D( d3ddev, &texDesc, NULL, &picsys->texture );
        if (FAILED(hr)) {
            msg_Err(va, "CreateTexture2D %d failed. (hr=0x%0lx)", pool_size, hr);
            goto error;
        }

        picsys->context = d3dctx;
        picsys->device = d3ddev;

        picture_resource_t resource = {
            .p_sys = picsys,
            .pf_destroy = DestroyD3D11Picture,
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
            DestroyD3D11Picture(pictures[i]);
        free(pictures);
    }
    return NULL;
}

static int Direct3D11MapTexture(picture_t *picture)
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr;
    int res;
    picture_sys_t *p_sys = picture->p_sys;
    hr = ID3D11DeviceContext_Map(picture->p_sys->context, (ID3D11Resource *)p_sys->texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if( FAILED(hr) )
        return VLC_EGENERIC;

    res = CommonUpdatePicture(picture, NULL, mappedResource.pData, mappedResource.RowPitch);
    ID3D11DeviceContext_Unmap(picture->p_sys->context,(ID3D11Resource *)p_sys->texture, 0);
    return res;
}

void DestroyD3D11Picture(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    ID3D11DeviceContext_Release(p_sys->context);

    if (p_sys->texture)
        ID3D11Texture2D_Release(p_sys->texture);

    FreeLibrary(p_sys->hd3d11_dll);

    free(p_sys);
    free(picture);
}

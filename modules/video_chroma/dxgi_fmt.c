/*****************************************************************************
 * d3d11_surface.c : D3D11 GPU surface conversion module for vlc
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

typedef struct
{
    const char   *name;
    DXGI_FORMAT  format;
} dxgi_format_t;

/* internal picture_t pool  */
typedef struct
{
    ID3D11Texture2D               *texture;
} picture_sys_pool_t;

struct picture_sys_t
{
    ID3D11VideoDecoderOutputView  *decoder; /* may be NULL for pictures from the pool */
    ID3D11Texture2D               *texture;
    ID3D11DeviceContext           *context;
};

static const dxgi_format_t dxgi_formats[] = {
    { "NV12",        DXGI_FORMAT_NV12                },
    { "I420_OPAQUE", DXGI_FORMAT_420_OPAQUE          },
    { "RGBA",        DXGI_FORMAT_R8G8B8A8_UNORM      },
    { "RGBA_SRGB",   DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
    { "BGRX",        DXGI_FORMAT_B8G8R8X8_UNORM      },
    { "BGRA",        DXGI_FORMAT_B8G8R8A8_UNORM      },
    { "BGRA_SRGB",   DXGI_FORMAT_B8G8R8A8_UNORM_SRGB },
    { "AYUV",        DXGI_FORMAT_AYUV                },
    { "YUY2",        DXGI_FORMAT_YUY2                },
    { "AI44",        DXGI_FORMAT_AI44                },
    { "P8",          DXGI_FORMAT_P8                  },
    { "A8P8",        DXGI_FORMAT_A8P8                },
    { "B5G6R5",      DXGI_FORMAT_B5G6R5_UNORM        },
    { "Y416",        DXGI_FORMAT_Y416                },
    { "P010",        DXGI_FORMAT_P010                },
    { "Y210",        DXGI_FORMAT_Y210                },
    { "Y410",        DXGI_FORMAT_Y410                },
    { "NV11",        DXGI_FORMAT_NV11                },
    { "UNKNOWN",     DXGI_FORMAT_UNKNOWN             },

    { NULL, 0,}
};

static const d3d_format_t d3d_formats[] = {
    { "I420",     DXGI_FORMAT_NV12,           VLC_CODEC_I420,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
    { "YV12",     DXGI_FORMAT_NV12,           VLC_CODEC_YV12,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
    { "NV12",     DXGI_FORMAT_NV12,           VLC_CODEC_NV12,     DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM },
    { "VA_NV12",  DXGI_FORMAT_NV12,           VLC_CODEC_D3D11_OPAQUE, DXGI_FORMAT_R8_UNORM,       DXGI_FORMAT_R8G8_UNORM },
//    { "VA_OPAQUE",DXGI_FORMAT_420_OPAQUE,     VLC_CODEC_DXGI_OPAQUE,  DXGI_FORMAT_R8_UNORM,       DXGI_FORMAT_R8G8_UNORM },
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
    { "VA_RGBA",  DXGI_FORMAT_R8G8B8A8_UNORM, VLC_CODEC_D3D11_OPAQUE, DXGI_FORMAT_R8G8B8A8_UNORM, 0 },
    { "B8G8R8A8", DXGI_FORMAT_B8G8R8A8_UNORM, VLC_CODEC_BGRA,     DXGI_FORMAT_B8G8R8A8_UNORM,     0 },
    { "VA_BGRA",  DXGI_FORMAT_B8G8R8A8_UNORM, VLC_CODEC_D3D11_OPAQUE, DXGI_FORMAT_B8G8R8A8_UNORM, 0 },
    { "R8G8B8X8", DXGI_FORMAT_B8G8R8X8_UNORM, VLC_CODEC_RGB32,    DXGI_FORMAT_B8G8R8X8_UNORM,     0 },
    { "B5G6R5",   DXGI_FORMAT_B5G6R5_UNORM,   VLC_CODEC_RGB16,    DXGI_FORMAT_B5G6R5_UNORM,       0 },

    { NULL, 0, 0, 0, 0}
};

const char *DxgiFormatToStr(DXGI_FORMAT format)
{
    for (const dxgi_format_t *f = dxgi_formats; f->name != NULL; ++f)
    {
        if (f->format == format)
            return f->name;
    }
    return NULL;
}

const d3d_format_t *GetRenderFormatList(void)
{
    return d3d_formats;
}

int D3D11CreateSurfaceContext( vlc_object_t *p_obj, picture_pool_d3d11 *sys )
{
    HRESULT hr;
    memset( sys, 0, sizeof(*sys) );
    sys->p_obj = p_obj;

    sys->hdecoder_dll = LoadLibrary( TEXT("D3D11.DLL") );
    if (!sys->hdecoder_dll) {
        msg_Warn( p_obj, "cannot load DirectX decoder DLL" );
        goto error;
    }
    msg_Dbg( p_obj, "DLLs loaded" );

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

picture_pool_t* D3D11CreateSurfacePool( vlc_object_t *p_obj, struct pool_picture_factory *p_pool_factory,
                                        const video_format_t *fmt, unsigned pool_size )
{
    VLC_UNUSED( p_obj );
    if ( fmt->i_chroma != VLC_CODEC_D3D11_OPAQUE )
        return NULL;

    picture_pool_d3d11 *sys = p_pool_factory->p_opaque;

    unsigned          picture_count = 0;
    picture_t**       pictures = NULL;
    HRESULT           hr;

    ID3D10Multithread *pMultithread;
    hr = ID3D11Device_QueryInterface( sys->d3ddevice, &IID_ID3D10Multithread, (void **)&pMultithread);
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
    texDesc.Format = sys->textureFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0; //D3D11_RESOURCE_MISC_SHARED;
    texDesc.ArraySize = 1;
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    for (picture_count = 0; picture_count < pool_size; picture_count++) {
        picture_sys_t *picsys = calloc(1, sizeof(*picsys));
        if (unlikely(picsys == NULL))
            goto error;

        hr = ID3D11Device_CreateTexture2D( sys->d3ddevice, &texDesc, NULL, &picsys->texture );
        if (FAILED(hr)) {
            msg_Err( sys->p_obj, "CreateTexture2D %d failed on picture %d of the pool. (hr=0x%0lx)", pool_size, picture_count, hr);
            goto error;
        }

        picsys->context = sys->d3dcontext;

        picture_resource_t resource = {
            .p_sys = picsys,
            .pf_destroy = DestroyDisplayPoolPicture,
        };

        picture_t *picture = picture_NewFromResource(fmt, &resource);
        if (unlikely(picture == NULL)) {
            free(picsys);
            msg_Err( sys->p_obj, "Failed to create picture %d in the pool.", picture_count );
            goto error;
        }

        pictures[picture_count] = picture;
        /* each picture_t holds a ref to the context and release it on Destroy */
        ID3D11DeviceContext_AddRef(picsys->context);
    }
    msg_Dbg( sys->p_obj, "ID3D11VideoDecoderOutputView succeed with %d surfaces (%dx%d)",
            pool_size, fmt->i_width, fmt->i_height);

    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = pool_size;
    pool_cfg.picture       = pictures;

    picture_pool_t *pool = picture_pool_NewExtended( &pool_cfg );

error:
    if (pool ==NULL && pictures) {
        msg_Dbg( sys->p_obj, "Failed to create the picture d3d11 pool");
        for (unsigned i=0;i<picture_count; ++i)
            DestroyDisplayPoolPicture(pictures[i]);
        free(pictures);

        /* create an empty pool to avoid crashing */
        picture_pool_configuration_t pool_cfg;
        memset( &pool_cfg, 0, sizeof( pool_cfg ) );
        pool_cfg.picture_count = 0;

        pool = picture_pool_NewExtended( &pool_cfg );
    }
    return pool;
}

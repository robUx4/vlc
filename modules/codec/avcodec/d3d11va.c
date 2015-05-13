/*****************************************************************************
 * d3d11va.c: Direct3D11 Video Acceleration helpers
 *****************************************************************************
 * Copyright (C) 2009 Geoffroy Couprie
 * Copyright (C) 2009 Laurent Aimar
 * Copyright (C) 2015 Steve Lhomme
 * $Id$
 *
 * Authors: Geoffroy Couprie <geal@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Steve Lhomme <robux4@gmail.com>
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

/**
  * See https://msdn.microsoft.com/en-us/library/windows/desktop/hh162912%28v=vs.85%29.aspx
  **/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define D3D11_DR 0

#include <assert.h>

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_fourcc.h>
#include <vlc_plugin.h>
#include <vlc_codecs.h>
#include <vlc_charset.h>

#include "directx_va.h"

#define COBJMACROS
#include <libavcodec/d3d11va.h>

#include "../../video_chroma/copy.h"

static int Open(vlc_va_t *, AVCodecContext *, enum PixelFormat,
                const es_format_t *, picture_sys_t *p_sys);
static void Close(vlc_va_t *, AVCodecContext *);
#if D3D11_DR
static ID3D11Device *GetOutputViewDevice(ID3D11VideoDecoderOutputView *p_view);
#endif

vlc_module_begin()
    set_description(N_("Direct3D11 Video Acceleration"))
    set_capability("hw decoder", 0)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_callbacks(Open, Close)
vlc_module_end()

#include <initguid.h> /* must be last included to not redefine existing GUIDs */

/* dxva2api.h GUIDs: http://msdn.microsoft.com/en-us/library/windows/desktop/ms697067(v=vs100).aspx
 * assume that they are declared in dxva2api.h */
#define MS_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)

#ifdef __MINGW32__
# include <_mingw.h>

# if !defined(__MINGW64_VERSION_MAJOR)
#  undef MS_GUID
#  define MS_GUID DEFINE_GUID /* dxva2api.h fails to declare those, redefine as static */
#  define DXVA2_E_NEW_VIDEO_DEVICE MAKE_HRESULT(1, 4, 4097)
# else
#  include <dxva.h>
# endif

#endif /* __MINGW32__ */

#if defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
#include <dxgidebug.h>
#endif

DEFINE_GUID(IID_ID3D11VideoDevice,   0x10EC4D5B, 0x975A, 0x4689, 0xB9, 0xE4, 0xD0, 0xAA, 0xC3, 0x0F, 0xE3, 0x33);
DEFINE_GUID(IID_ID3D11VideoContext,  0x61F21C45, 0x3C0E, 0x4a74, 0x9C, 0xEA, 0x67, 0x10, 0x0D, 0x9A, 0xD5, 0xE4);
DEFINE_GUID(IID_IDXGIDevice,         0x54ec77fa, 0x1377, 0x44e6, 0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c);
DEFINE_GUID(IID_ID3D10Multithread,   0x9b7e4e00, 0x342c, 0x4106, 0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0);

/* Codec capabilities GUID, sorted by codec */
DEFINE_GUID(D3D11_DECODER_PROFILE_MPEG2_MOCOMP,      0xe6a9f44b, 0x61b0, 0x4563,0x9e,0xa4,0x63,0xd2,0xa3,0xc6,0xfe,0x66);
DEFINE_GUID(D3D11_DECODER_PROFILE_MPEG2_IDCT,        0xbf22ad00, 0x03ea, 0x4690,0x80,0x77,0x47,0x33,0x46,0x20,0x9b,0x7e);
DEFINE_GUID(D3D11_DECODER_PROFILE_MPEG2_VLD,         0xee27417f, 0x5e28, 0x4e65,0xbe,0xea,0x1d,0x26,0xb5,0x08,0xad,0xc9);
DEFINE_GUID(D3D11_DECODER_PROFILE_MPEG1_VLD,         0x6f3ec719, 0x3735, 0x42cc,0x80,0x63,0x65,0xcc,0x3c,0xb3,0x66,0x16);
DEFINE_GUID(D3D11_DECODER_PROFILE_MPEG2and1_VLD,     0x86695f12, 0x340e, 0x4f04,0x9f,0xd3,0x92,0x53,0xdd,0x32,0x74,0x60);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_MOCOMP_NOFGT, 0x1b81be64, 0xa0c7, 0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_MOCOMP_FGT,   0x1b81be65, 0xa0c7, 0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_IDCT_NOFGT,   0x1b81be66, 0xa0c7, 0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_IDCT_FGT,     0x1b81be67, 0xa0c7, 0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_VLD_NOFGT,    0x1b81be68, 0xa0c7, 0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_VLD_FGT,      0x1b81be69, 0xa0c7, 0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_VLD_WITHFMOASO_NOFGT,  0xd5f04ff9, 0x3418,0x45d8,0x95,0x61,0x32,0xa7,0x6a,0xae,0x2d,0xdd);
DEFINE_GUID(DXVA_Intel_H264_NoFGT_ClearVideo,        0x604F8E68, 0x4951, 0x4c54,0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6);
DEFINE_GUID(D3D11_DECODER_PROFILE_WMV8_POSTPROC,     0x1b81be80, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_WMV8_MOCOMP,       0x1b81be81, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_WMV9_POSTPROC,     0x1b81be90, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_WMV9_MOCOMP,       0x1b81be91, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_WMV9_IDCT,         0x1b81be94, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_VC1_POSTPROC,      0x1b81beA0, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_VC1_MOCOMP,        0x1b81beA1, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_VC1_IDCT,          0x1b81beA2, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_VC1_VLD,           0x1b81beA3, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_VC1_D2010,         0x1b81beA4, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(D3D11_DECODER_PROFILE_MPEG4PT2_VLD_SIMPLE,           0xefd64d74, 0xc9e8,0x41d7,0xa5,0xe9,0xe9,0xb0,0xe3,0x9f,0xa3,0x19);
DEFINE_GUID(D3D11_DECODER_PROFILE_MPEG4PT2_VLD_ADVSIMPLE_NOGMC,  0xed418a9f, 0x010d,0x4eda,0x9a,0xe3,0x9a,0x65,0x35,0x8d,0x8d,0x2e);
DEFINE_GUID(D3D11_DECODER_PROFILE_MPEG4PT2_VLD_ADVSIMPLE_GMC,    0xab998b5b, 0x4258,0x44a9,0x9f,0xeb,0x94,0xe5,0x97,0xa6,0xba,0xae);
DEFINE_GUID(D3D11_DECODER_PROFILE_HEVC_VLD_MAIN,     0x5b11d51b, 0x2f4c,0x4452,0xbc,0xc3,0x09,0xf2,0xa1,0x16,0x0c,0xc0);
DEFINE_GUID(D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10,   0x107af0e0, 0xef1a,0x4d19,0xab,0xa8,0x67,0xa1,0x63,0x07,0x3d,0x13);

DEFINE_GUID(D3D11_DECODER_PROFILE_H264_VLD_STEREO_PROGRESSIVE_NOFGT, 0xd79be8da, 0x0cf1,0x4c81,0xb8,0x2a,0x69,0xa4,0xe2,0x36,0xf4,0x3d);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_VLD_STEREO_NOFGT,             0xf9aaccbb, 0xc2b6,0x4cfc,0x87,0x79,0x57,0x07,0xb1,0x76,0x05,0x52);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_VLD_MULTIVIEW_NOFGT,          0x705b9d82, 0x76cf,0x49d6,0xb7,0xe6,0xac,0x88,0x72,0xdb,0x01,0x3c);

/* XXX Prefered modes must come first */
static const directx_va_mode_t dxva_modes[] = {
    /* MPEG-1/2 */
    { "MPEG-2 variable-length decoder",                                               &D3D11_DECODER_PROFILE_MPEG2_VLD,         AV_CODEC_ID_MPEG2VIDEO, PROF_MPEG2_SIMPLE },
    { "MPEG-2 & MPEG-1 variable-length decoder",                                      &D3D11_DECODER_PROFILE_MPEG2and1_VLD,     AV_CODEC_ID_MPEG2VIDEO, PROF_MPEG2_MAIN },
    { "MPEG-2 & MPEG-1 variable-length decoder",                                      &D3D11_DECODER_PROFILE_MPEG2and1_VLD,     AV_CODEC_ID_MPEG1VIDEO, NULL },
    { "MPEG-2 motion compensation",                                                   &D3D11_DECODER_PROFILE_MPEG2_MOCOMP,      0, NULL },
    { "MPEG-2 inverse discrete cosine transform",                                     &D3D11_DECODER_PROFILE_MPEG2_IDCT,        0, NULL },

    /* MPEG-1 http://download.microsoft.com/download/B/1/7/B172A3C8-56F2-4210-80F1-A97BEA9182ED/DXVA_MPEG1_VLD.pdf */
    { "MPEG-1 variable-length decoder, no D pictures",                                &D3D11_DECODER_PROFILE_MPEG1_VLD,         0, NULL },

    /* H.264 http://www.microsoft.com/downloads/details.aspx?displaylang=en&FamilyID=3d1c290b-310b-4ea2-bf76-714063a6d7a6 */
    { "H.264 variable-length decoder, film grain technology",                         &D3D11_DECODER_PROFILE_H264_VLD_FGT,      AV_CODEC_ID_H264, PROF_H264_HIGH },
    { "H.264 variable-length decoder, no film grain technology (Intel ClearVideo)",   &DXVA_Intel_H264_NoFGT_ClearVideo,        AV_CODEC_ID_H264, PROF_H264_HIGH },
    { "H.264 variable-length decoder, no film grain technology",                      &D3D11_DECODER_PROFILE_H264_VLD_NOFGT,    AV_CODEC_ID_H264, PROF_H264_HIGH },
    { "H.264 variable-length decoder, no film grain technology, FMO/ASO",             &D3D11_DECODER_PROFILE_H264_VLD_WITHFMOASO_NOFGT, AV_CODEC_ID_H264, PROF_H264_HIGH },

    { "H.264 inverse discrete cosine transform, film grain technology",               &D3D11_DECODER_PROFILE_H264_IDCT_FGT,     0, NULL },
    { "H.264 inverse discrete cosine transform, no film grain technology",            &D3D11_DECODER_PROFILE_H264_IDCT_NOFGT,   0, NULL },

    { "H.264 motion compensation, film grain technology",                             &D3D11_DECODER_PROFILE_H264_MOCOMP_FGT,   0, NULL },
    { "H.264 motion compensation, no film grain technology",                          &D3D11_DECODER_PROFILE_H264_MOCOMP_NOFGT, 0, NULL },

    /* http://download.microsoft.com/download/2/D/0/2D02E72E-7890-430F-BA91-4A363F72F8C8/DXVA_H264_MVC.pdf */
    { "H.264 stereo high profile, mbs flag set",                                      &D3D11_DECODER_PROFILE_H264_VLD_STEREO_PROGRESSIVE_NOFGT, 0, NULL },
    { "H.264 stereo high profile",                                                    &D3D11_DECODER_PROFILE_H264_VLD_STEREO_NOFGT,             0, NULL },
    { "H.264 multiview high profile",                                                 &D3D11_DECODER_PROFILE_H264_VLD_MULTIVIEW_NOFGT,          0, NULL },

    /* WMV */
    { "Windows Media Video 8 motion compensation",                                    &D3D11_DECODER_PROFILE_WMV8_MOCOMP,       0, NULL },
    { "Windows Media Video 8 post processing",                                        &D3D11_DECODER_PROFILE_WMV8_POSTPROC,     0, NULL },

    { "Windows Media Video 9 IDCT",                                                   &D3D11_DECODER_PROFILE_WMV9_IDCT,         0, NULL },
    { "Windows Media Video 9 motion compensation",                                    &D3D11_DECODER_PROFILE_WMV9_MOCOMP,       0, NULL },
    { "Windows Media Video 9 post processing",                                        &D3D11_DECODER_PROFILE_WMV9_POSTPROC,     0, NULL },

    /* VC-1 */
    { "VC-1 variable-length decoder",                                                 &D3D11_DECODER_PROFILE_VC1_VLD,           AV_CODEC_ID_VC1, NULL },
    { "VC-1 variable-length decoder",                                                 &D3D11_DECODER_PROFILE_VC1_VLD,           AV_CODEC_ID_WMV3, NULL },
    { "VC-1 variable-length decoder",                                                 &D3D11_DECODER_PROFILE_VC1_D2010,         AV_CODEC_ID_VC1, NULL },
    { "VC-1 variable-length decoder",                                                 &D3D11_DECODER_PROFILE_VC1_D2010,         AV_CODEC_ID_WMV3, NULL },

    { "VC-1 inverse discrete cosine transform",                                       &D3D11_DECODER_PROFILE_VC1_IDCT,          0, NULL },
    { "VC-1 motion compensation",                                                     &D3D11_DECODER_PROFILE_VC1_MOCOMP,        0, NULL },
    { "VC-1 post processing",                                                         &D3D11_DECODER_PROFILE_VC1_POSTPROC,      0, NULL },

    /* Xvid/Divx: TODO */
    { "MPEG-4 Part 2 variable-length decoder, Simple Profile",                        &D3D11_DECODER_PROFILE_MPEG4PT2_VLD_SIMPLE, 0, NULL },
    { "MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, no GMC",       &D3D11_DECODER_PROFILE_MPEG4PT2_VLD_ADVSIMPLE_NOGMC, 0, NULL },
    { "MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, GMC",          &D3D11_DECODER_PROFILE_MPEG4PT2_VLD_ADVSIMPLE_GMC,   0, NULL },

    /* HEVC */
    { "HEVC Main profile",                                                            &D3D11_DECODER_PROFILE_HEVC_VLD_MAIN,      AV_CODEC_ID_HEVC, PROF_HEVC_MAIN },
    { "HEVC Main 10 profile",                                                         &D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10,    AV_CODEC_ID_HEVC, PROF_HEVC_MAIN10 },

    { NULL, NULL, 0, NULL }
};

/* */
typedef struct {
    const char   *name;
    DXGI_FORMAT  format;
    vlc_fourcc_t codec;
} d3d_format_t;
/* XXX Prefered format must come first */
static const d3d_format_t d3d_formats[] = {
    { "NV12",     DXGI_FORMAT_NV12,           VLC_CODEC_NV12 },
    { "I420L",    DXGI_FORMAT_P010,           VLC_CODEC_I420_10L },
    { "YUY2",     DXGI_FORMAT_YUY2,           VLC_CODEC_I420_10L },
    { "B8G8R8A8", DXGI_FORMAT_B8G8R8A8_UNORM, VLC_CODEC_BGRA },

    { NULL, 0, 0 }
};

struct vlc_va_sys_t
{
    directx_sys_t                dx_sys;

#if defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    HINSTANCE                    dxgidebug_dll;
#endif

    /* Video service */
    ID3D11VideoContext           *d3dvidctx;
    DXGI_FORMAT                  render;

    ID3D11DeviceContext          *d3dctx;

    /* Video decoder */
    D3D11_VIDEO_DECODER_CONFIG   cfg;

    /* avcodec internals */
    struct AVD3D11VAContext      hw;

    /* Extraction */
    ID3D11Resource               *staging;
    copy_cache_t                 *p_copy_cache;
};

#if D3D11_DR /* for now we export to NV12 */
struct picture_sys_t
{
    ID3D11VideoDecoderOutputView *surface;
};
#endif

/* */
static int D3dCreateDevice(vlc_va_t *);
static void D3dDestroyDevice(vlc_va_t *);
static char *DxDescribe(directx_sys_t *);

static int D3dCreateDeviceManager(vlc_va_t *);
static void D3dDestroyDeviceManager(vlc_va_t *);

static int DxCreateVideoService(vlc_va_t *);
static void DxDestroyVideoService(vlc_va_t *);
static int DxFindVideoServiceConversion(vlc_va_t *, GUID *input, const es_format_t *fmt);

static int DxCreateDecoderSurfaces(vlc_va_t *va, int codec_id, const video_format_t *fmt, bool b_threading);
static void DxDestroySurfaces(vlc_va_t *);
static void SetupAVCodecContext(vlc_va_t *);

/* */
static int Setup(vlc_va_t *va, AVCodecContext *avctx, vlc_fourcc_t *chroma)
{
    vlc_va_sys_t *sys = va->sys;
    if (directx_va_Setup(va, &sys->dx_sys, avctx, chroma)!=VLC_SUCCESS)
        return VLC_EGENERIC;

    avctx->hwaccel_context = &sys->hw;
    *chroma = VLC_CODEC_NV12; /* TODO use an opaque format for direct rendering */

    return VLC_SUCCESS;
}

void SetupAVCodecContext(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    directx_sys_t *dx_sys = &sys->dx_sys;
    sys->hw.video_context = sys->d3dvidctx;
    sys->hw.decoder = (ID3D11VideoDecoder*) dx_sys->decoder;
    sys->hw.cfg = &sys->cfg;
    sys->hw.surface_count = dx_sys->surface_count;
    sys->hw.surface = (ID3D11VideoDecoderOutputView**) dx_sys->hw_surface;
}

static int Extract(vlc_va_t *va, picture_t *picture, uint8_t *data)
{
    vlc_va_sys_t *sys = va->sys;
    ID3D11VideoDecoderOutputView *d3d = (ID3D11VideoDecoderOutputView*)(uintptr_t)data;
    ID3D11Resource *p_texture = NULL;
    D3D11_TEXTURE2D_DESC texDesc;
    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr;

    assert( picture->format.i_chroma == VLC_CODEC_NV12);

    ID3D11VideoDecoderOutputView_GetDesc( d3d, &viewDesc );

    ID3D11VideoDecoderOutputView_GetResource( d3d, &p_texture );
    if (!p_texture) {
        msg_Err(va, "Failed to get the texture of the outputview." );
        goto error;
    }

    ID3D11Texture2D_GetDesc( (ID3D11Texture2D*) p_texture, &texDesc);

    /* extract to NV12 planes */
    ID3D11DeviceContext_CopySubresourceRegion(sys->d3dctx, sys->staging, 0, 0, 0, 0,
                                              p_texture, viewDesc.Texture2D.ArraySlice, NULL);

    hr = ID3D11DeviceContext_Map(sys->d3dctx, sys->staging, 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        msg_Err(va, "Failed to map the texture surface pixels (hr=0x%0lx)", hr );
        goto error;
    }

    uint8_t *plane[2] = {
        mappedResource.pData,
        (uint8_t*)mappedResource.pData + mappedResource.RowPitch * texDesc.Height,
    };
    size_t  pitch[2] = {
        mappedResource.RowPitch,
        mappedResource.RowPitch,
    };
    CopyFromNv12ToNv12(picture, plane, pitch, texDesc.Width, texDesc.Height, sys->p_copy_cache );

    ID3D11DeviceContext_Unmap(sys->d3dctx, sys->staging, 0);

    if (p_texture!=NULL)
        ID3D11Resource_Release(p_texture);
    return VLC_SUCCESS;

error:
    if (p_texture!=NULL)
        ID3D11Resource_Release(p_texture);
    return VLC_EGENERIC;
}

static int CheckDevice(vlc_va_t *va)
{
    VLC_UNUSED(va);
#ifdef TODO
    /* Check the device */
    /* see MFCreateDXGIDeviceManager in mfplat.dll, not avail in Win7 */
    HRESULT hr = IDirect3DDeviceManager9_TestDevice(sys->devmng, sys->device);
    if (hr == DXVA2_E_NEW_VIDEO_DEVICE) {
        if (DxResetVideoDecoder(va))
            return VLC_EGENERIC;
    } else if (FAILED(hr)) {
        msg_Err(va, "IDirect3DDeviceManager9_TestDevice %u", (unsigned)hr);
        return VLC_EGENERIC;
    }
#endif
    return VLC_SUCCESS;
}

/* FIXME it is nearly common with VAAPI */
static int Get(vlc_va_t *va, picture_t *pic, uint8_t **data)
{
    return directx_va_Get(va, &va->sys->dx_sys, pic, data);
}

static void Release(void *opaque, uint8_t *data)
{
    directx_va_Release(opaque, data);
}

#if D3D11_DR
static ID3D11Device *GetOutputViewDevice(ID3D11VideoDecoderOutputView *p_view)
{
    ID3D11Device *result = NULL;
    ID3D11Resource *p_texture;
    ID3D11VideoDecoderOutputView_GetResource( p_view, &p_texture );
    if (p_texture) {
        ID3D11Texture2D_GetDevice( (ID3D11Texture2D*)p_texture, &result );
        ID3D11Resource_Release( p_texture );
        if (result)
            ID3D11Device_Release(result);
    }
    return result;
}
#endif

static void Close(vlc_va_t *va, AVCodecContext *ctx)
{
    vlc_va_sys_t *sys = va->sys;

    (void) ctx;

    directx_va_Close(va, &sys->dx_sys);

    if (sys->p_copy_cache!=NULL) {
        CopyCleanCache(sys->p_copy_cache);
        free( sys->p_copy_cache );
    }

#if defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    if (sys->dxgidebug_dll)
        FreeLibrary(sys->dxgidebug_dll);
#if DEBUG_LEAK
    va->sys->dxgidebug_dll = NULL;
#endif
#endif

    free((char *)va->description);
    free(sys);
}

static int Open(vlc_va_t *va, AVCodecContext *ctx, enum PixelFormat pix_fmt,
                const es_format_t *fmt, picture_sys_t *p_sys)
{
    VLC_UNUSED(p_sys);
    int err = VLC_EGENERIC;
    directx_sys_t *dx_sys;

    if (pix_fmt != AV_PIX_FMT_D3D11VA_VLD)
        return VLC_EGENERIC;

    vlc_va_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->p_copy_cache = calloc(1, sizeof(*sys->p_copy_cache));
    if (!sys->p_copy_cache) {
         err = VLC_ENOMEM;
         goto error;
    }

#if defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    sys->dxgidebug_dll = LoadLibrary(TEXT("DXGIDEBUG.DLL"));
#endif

    va->setup   = Setup;
    va->get     = Get;
    va->release = Release;
    va->extract = Extract;

    dx_sys = &sys->dx_sys;

    dx_sys->pf_check_device            = CheckDevice;
    dx_sys->pf_create_device           = D3dCreateDevice;
    dx_sys->pf_destroy_device          = D3dDestroyDevice;
    dx_sys->pf_create_device_manager   = D3dCreateDeviceManager;
    dx_sys->pf_destroy_device_manager  = D3dDestroyDeviceManager;
    dx_sys->pf_create_video_service    = DxCreateVideoService;
    dx_sys->pf_destroy_video_service   = DxDestroyVideoService;
    dx_sys->pf_create_decoder_surfaces = DxCreateDecoderSurfaces;
    dx_sys->pf_destroy_surfaces        = DxDestroySurfaces;
    dx_sys->pf_setup_avcodec_ctx       = SetupAVCodecContext;
    dx_sys->pf_find_service_conversion = DxFindVideoServiceConversion;
    dx_sys->psz_decoder_dll            = TEXT("D3D11.DLL");

    va->sys = sys;

    dx_sys->d3ddev = NULL;
#if D3D11_DR
    if ( p_sys != NULL )
        dx_sys->d3ddev = GetOutputViewDevice( p_sys->surface );
#endif

    err = directx_va_Open(va, &sys->dx_sys, ctx, fmt);
    if (err!=VLC_SUCCESS)
        goto error;

    CopyInitCache( sys->p_copy_cache, fmt->video.i_width );

    /* TODO print the hardware name/vendor for debugging purposes */
    va->description = DxDescribe(dx_sys);

    return VLC_SUCCESS;

error:
    Close(va, ctx);
    return err;
}

/**
 * It creates a Direct3D device usable for decoding
 */
static int D3dCreateDevice(vlc_va_t *va)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    HRESULT hr;

    /* */
    PFN_D3D11_CREATE_DEVICE pf_CreateDevice;
    pf_CreateDevice = (void *)GetProcAddress(dx_sys->hdecoder_dll, "D3D11CreateDevice");
    if (!pf_CreateDevice) {
        msg_Err(va, "Cannot locate reference to D3D11CreateDevice ABI in DLL");
        return VLC_EGENERIC;
    }

    UINT creationFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
# if !defined(NDEBUG) //&& defined(_MSC_VER)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
# endif

    /* */
    ID3D11Device *d3ddev;
    ID3D11DeviceContext *d3dctx;
    hr = pf_CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                                 creationFlags, NULL, 0,
                                 D3D11_SDK_VERSION, &d3ddev, NULL, &d3dctx);
    if (FAILED(hr)) {
        msg_Err(va, "D3D11CreateDevice failed. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    dx_sys->d3ddev = (IUnknown*) d3ddev;
    va->sys->d3dctx = d3dctx;
#if DEBUG_LEAK
    msg_Err(va, "Created ID3D11Device 0x%p / ID3D11DeviceContext 0x%p", d3ddev, d3dctx);
#endif

    ID3D11VideoContext *d3dvidctx = NULL;
    hr = ID3D11DeviceContext_QueryInterface(d3dctx, &IID_ID3D11VideoContext, (void **)&d3dvidctx);
    if (FAILED(hr)) {
       msg_Err(va, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    va->sys->d3dvidctx = d3dvidctx;
#if DEBUG_LEAK
    msg_Err(va, "Got ID3D11VideoContext 0x%p", d3dvidctx);
#endif

#if defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    HRESULT (WINAPI  * pf_DXGIGetDebugInterface)(const GUID *riid, void **ppDebug);
    if (sys->dxgidebug_dll) {
        pf_DXGIGetDebugInterface = (void *)GetProcAddress(sys->dxgidebug_dll, "DXGIGetDebugInterface");
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
}

/**
 * It releases a Direct3D device and its resources.
 */
static void D3dDestroyDevice(vlc_va_t *va)
{
    if (va->sys->d3dvidctx)
        ID3D11VideoContext_Release(va->sys->d3dvidctx);
    if (va->sys->d3dctx)
        ID3D11DeviceContext_Release(va->sys->d3dctx);
#if DEBUG_LEAK
    va->sys->d3dvidctx = NULL;
    va->sys->d3dctx = NULL;
#endif
}
/**
 * It describes our Direct3D object
 */
static char *DxDescribe(directx_sys_t *dx_sys)
{
    static const struct {
        unsigned id;
        char     name[32];
    } vendors [] = {
        { 0x1002, "ATI" },
        { 0x10DE, "NVIDIA" },
        { 0x1106, "VIA" },
        { 0x8086, "Intel" },
        { 0x5333, "S3 Graphics" },
        { 0, "" }
    };

    IDXGIDevice *pDXGIDevice = NULL;
    HRESULT hr = ID3D11Device_QueryInterface( (ID3D11Device*) dx_sys->d3ddev, &IID_IDXGIDevice, (void **)&pDXGIDevice);
    if (FAILED(hr)) {
       return NULL;
    }

    IDXGIAdapter *p_adapter;
    hr = IDXGIDevice_GetAdapter(pDXGIDevice, &p_adapter);
    if (FAILED(hr)) {
        IDXGIDevice_Release(pDXGIDevice);
       return NULL;
    }

    DXGI_ADAPTER_DESC adapterDesc;
    if (SUCCEEDED(IDXGIAdapter_GetDesc(p_adapter, &adapterDesc))) {
        const char *vendor = "Unknown";
        for (int i = 0; vendors[i].id != 0; i++) {
            if (vendors[i].id == adapterDesc.VendorId) {
                vendor = vendors[i].name;
                break;
            }
        }

        char *description;
        if (asprintf(&description, "D3D11VA (%s, vendor %u(%s), device %u, revision %u)",
                     FromWide(adapterDesc.Description),
                     adapterDesc.VendorId, vendor, adapterDesc.DeviceId, adapterDesc.Revision) < 0)
            return NULL;
        IDXGIAdapter_Release(p_adapter);
        IDXGIDevice_Release(pDXGIDevice);
        return description;
    }

    IDXGIAdapter_Release(p_adapter);
    IDXGIDevice_Release(pDXGIDevice);
    return NULL;
}

/**
 * It creates a Direct3D device manager
 */
static int D3dCreateDeviceManager(vlc_va_t *va)
{
    VLC_UNUSED(va);
#if 0
    vlc_va_sys_t *sys = va->sys;

    HRESULT (WINAPI *CreateDeviceManager9)(UINT *pResetToken,
                                           IDirect3DDeviceManager9 **);
    CreateDeviceManager9 =
      (void *)GetProcAddress(sys->hdxva2_dll,
                             "DXVA2CreateDirect3DDeviceManager9");

    if (!CreateDeviceManager9) {
        msg_Err(va, "cannot load function");
        return VLC_EGENERIC;
    }
    msg_Dbg(va, "OurDirect3DCreateDeviceManager9 Success!");

    UINT token;
    IDirect3DDeviceManager9 *devmng;
    if (FAILED(CreateDeviceManager9(&token, &devmng))) {
        msg_Err(va, " OurDirect3DCreateDeviceManager9 failed");
        return VLC_EGENERIC;
    }
    sys->token  = token;
    sys->devmng = devmng;
    msg_Info(va, "obtained IDirect3DDeviceManager9");

    HRESULT hr = IDirect3DDeviceManager9_ResetDevice(devmng, (ID3D11Device*) dx_sys->d3ddev, token);
    if (FAILED(hr)) {
        msg_Err(va, "IDirect3DDeviceManager9_ResetDevice failed: %08x", (unsigned)hr);
        return VLC_EGENERIC;
    }
#endif
    return VLC_SUCCESS;
}
/**
 * It destroys a Direct3D device manager
 */
static void D3dDestroyDeviceManager(vlc_va_t *va)
{
    VLC_UNUSED(va);
#if 0
    if (va->devmng)
        IDirect3DDeviceManager9_Release(va->devmng);
#endif
}

/**
 * It creates a DirectX video service
 */
static int DxCreateVideoService(vlc_va_t *va)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;

    ID3D11VideoDevice *d3dviddev = NULL;
    HRESULT hr = ID3D11Device_QueryInterface( (ID3D11Device*) dx_sys->d3ddev, &IID_ID3D11VideoDevice, (void **)&d3dviddev);
    if (FAILED(hr)) {
       msg_Err(va, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    dx_sys->d3ddec = (IUnknown*) d3dviddev;
#if DEBUG_LEAK
    msg_Err(va, "Got ID3D11VideoDevice 0x%p", d3dviddev);
#endif

    return VLC_SUCCESS;
}

/**
 * It destroys a DirectX video service
 */
static void DxDestroyVideoService(vlc_va_t *va)
{
    VLC_UNUSED(va);
}

/**
 * Find the best suited decoder mode GUID and render format.
 */
static int DxFindVideoServiceConversion(vlc_va_t *va, GUID *input, const es_format_t *fmt)
{
    vlc_va_sys_t *sys = va->sys;
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    HRESULT hr;

    /* Retreive supported modes from the decoder service */
    UINT input_count = ID3D11VideoDevice_GetVideoDecoderProfileCount((ID3D11VideoDevice*) dx_sys->d3ddec);
    GUID input_list[input_count];
    memset(input_list, 0, sizeof(input_list));
    for (UINT i = 0; i < input_count; i++) {
        hr = ID3D11VideoDevice_GetVideoDecoderProfile((ID3D11VideoDevice*) dx_sys->d3ddec, i, &input_list[i]);
        if (FAILED(hr))
        {
            msg_Err(va, "GetVideoDecoderProfile %d failed. (hr=0x%lX)", i, hr);
            continue;
        }
    }
    for (unsigned i = 0; i < input_count; i++) {
        const GUID *g = &input_list[i];
        const directx_va_mode_t *mode = directx_va_FindMode(g, dxva_modes);
        if (mode) {
            msg_Dbg(va, "- '%s' is supported by hardware", mode->name);
        } else {
            msg_Warn(va, "- Unknown GUID = " GUID_FMT, GUID_PRINT( *g ) );
        }
    }

    /* Try all supported mode by our priority */
    for (unsigned i = 0; dxva_modes[i].name; i++) {
        const directx_va_mode_t *mode = &dxva_modes[i];
        if (!mode->codec || mode->codec != dx_sys->codec_id)
            continue;

        /* */
        bool is_supported = false;
        for (const GUID *g = &input_list[0]; !is_supported && g < &input_list[input_count]; g++) {
            is_supported = IsEqualGUID(mode->guid, g);
        }
        if ( is_supported )
        {
            is_supported = directx_va_ProfileSupported( mode, fmt );
            if (!is_supported)
                msg_Warn( va, "Unsupported profile for D3D11 HWAccel: %d", fmt->i_profile );
        }
        if (!is_supported)
            continue;

        /* */
        msg_Dbg(va, "Trying to use '%s' as input", mode->name);
        for (unsigned j = 0; d3d_formats[j].name; j++) {
            const d3d_format_t *format = &d3d_formats[j];

            BOOL is_supported = false;
            hr = ID3D11VideoDevice_CheckVideoDecoderFormat((ID3D11VideoDevice*) dx_sys->d3ddec, mode->guid, format->format, &is_supported);
            if (FAILED(hr) || !is_supported)
                continue;
            msg_Dbg(va, "%s is supported for output", format->name);
        }

        /* */
        for (unsigned j = 0; d3d_formats[j].name; j++) {
            const d3d_format_t *format = &d3d_formats[j];

            /* */
            BOOL is_supported = false;
            hr = ID3D11VideoDevice_CheckVideoDecoderFormat((ID3D11VideoDevice*) dx_sys->d3ddec, mode->guid, format->format, &is_supported);
            if (FAILED(hr) || !is_supported)
                continue;

            /* We have our solution */
            msg_Dbg(va, "Using '%s' to decode to '%s'", mode->name, format->name);
            *input  = *mode->guid;
            sys->render = format->format;
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

/**
 * It creates a Direct3D11 decoder using the given video format
 */
static int DxCreateDecoderSurfaces(vlc_va_t *va, int codec_id, const video_format_t *fmt, bool b_threading)
{
    VLC_UNUSED(fmt);

    vlc_va_sys_t *sys = va->sys;
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    HRESULT hr;

    ID3D10Multithread *pMultithread;
    hr = ID3D11Device_QueryInterface( (ID3D11Device*) dx_sys->d3ddev, &IID_ID3D10Multithread, (void **)&pMultithread);
    if (SUCCEEDED(hr)) {
        ID3D10Multithread_SetMultithreadProtected(pMultithread, b_threading && dx_sys->thread_count > 1);
        ID3D10Multithread_Release(pMultithread);
#if DEBUG_LEAK
    msg_Err(va, "Got ID3D10Multithread 0x%p", pMultithread);
#endif
    }

    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width = dx_sys->surface_width;
    texDesc.Height = dx_sys->surface_height;
    texDesc.MipLevels = 1;
    texDesc.Format = sys->render;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0; // D3D11_RESOURCE_MISC_SHARED
    texDesc.ArraySize = 1;
    texDesc.Usage = D3D11_USAGE_STAGING;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    texDesc.BindFlags = 0;

    sys->staging = NULL;
    hr = ID3D11Device_CreateTexture2D( (ID3D11Device*) dx_sys->d3ddev, &texDesc, NULL,
                                       (ID3D11Texture2D**) &sys->staging);
    if (FAILED(hr)) {
        msg_Err(va, "Failed to create a staging texture to extract surface pixels (hr=0x%0lx)", hr );
        return VLC_EGENERIC;
    }

    texDesc.ArraySize = dx_sys->surface_count;
    texDesc.Usage = D3D11_USAGE_DEFAULT; //D3D11_USAGE_DYNAMIC; //D3D11_USAGE_STAGING; // D3D11_USAGE_DEFAULT
    texDesc.BindFlags = D3D11_BIND_DECODER;// | D3D11_BIND_UNORDERED_ACCESS;
    texDesc.CPUAccessFlags = 0; //D3D11_CPU_ACCESS_READ;

    ID3D11Texture2D *p_texture;
    hr = ID3D11Device_CreateTexture2D( (ID3D11Device*) dx_sys->d3ddev, &texDesc, NULL, &p_texture );
    if (FAILED(hr)) {
        msg_Err(va, "CreateTexture2D %d failed. (hr=0x%0lx)", dx_sys->surface_count, hr);
        return VLC_EGENERIC;
    }

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    ZeroMemory(&viewDesc, sizeof(viewDesc));
    viewDesc.DecodeProfile = dx_sys->input;
    viewDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;

    int surface_count = dx_sys->surface_count;
    for (dx_sys->surface_count = 0; dx_sys->surface_count < surface_count; dx_sys->surface_count++) {
        viewDesc.Texture2D.ArraySlice = dx_sys->surface_count;

        hr = ID3D11VideoDevice_CreateVideoDecoderOutputView( (ID3D11VideoDevice*) dx_sys->d3ddec,
                                                             (ID3D11Resource*) p_texture,
                                                             &viewDesc,
                                                             (ID3D11VideoDecoderOutputView**) &dx_sys->hw_surface[dx_sys->surface_count] );
        if (FAILED(hr)) {
            msg_Err(va, "CreateVideoDecoderOutputView %d failed. (hr=0x%0lx)", dx_sys->surface_count, hr);
            ID3D11Texture2D_Release(p_texture);
            return VLC_EGENERIC;
        }
    }
    msg_Dbg(va, "ID3D11VideoDecoderOutputView succeed with %d surfaces (%dx%d)",
            dx_sys->surface_count, dx_sys->surface_width, dx_sys->surface_height);

    D3D11_VIDEO_DECODER_DESC decoderDesc;
    ZeroMemory(&decoderDesc, sizeof(decoderDesc));
    decoderDesc.Guid = dx_sys->input;
    decoderDesc.SampleWidth = dx_sys->surface_width;
    decoderDesc.SampleHeight = dx_sys->surface_height;
    decoderDesc.OutputFormat = sys->render;

    UINT cfg_count;
    hr = ID3D11VideoDevice_GetVideoDecoderConfigCount( (ID3D11VideoDevice*) dx_sys->d3ddec, &decoderDesc, &cfg_count );
    if (FAILED(hr)) {
        msg_Err(va, "GetVideoDecoderConfigCount failed. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    /* List all configurations available for the decoder */
    D3D11_VIDEO_DECODER_CONFIG cfg_list[cfg_count];
    for (unsigned i = 0; i < cfg_count; i++) {
        hr = ID3D11VideoDevice_GetVideoDecoderConfig( (ID3D11VideoDevice*) dx_sys->d3ddec, &decoderDesc, i, &cfg_list[i] );
        if (FAILED(hr)) {
            msg_Err(va, "GetVideoDecoderConfig failed. (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
    }

    msg_Dbg(va, "we got %d decoder configurations", cfg_count);

    /* Select the best decoder configuration */
    int cfg_score = 0;
    for (unsigned i = 0; i < cfg_count; i++) {
        const D3D11_VIDEO_DECODER_CONFIG *cfg = &cfg_list[i];

        /* */
        msg_Dbg(va, "configuration[%d] ConfigBitstreamRaw %d",
                i, cfg->ConfigBitstreamRaw);

        /* */
        int score;
        if (cfg->ConfigBitstreamRaw == 1)
            score = 1;
        else if (codec_id == AV_CODEC_ID_H264 && cfg->ConfigBitstreamRaw == 2)
            score = 2;
        else
            continue;
        if (IsEqualGUID(&cfg->guidConfigBitstreamEncryption, &DXVA_NoEncrypt))
            score += 16;

        if (cfg_score < score) {
            sys->cfg = *cfg;
            cfg_score = score;
        }
    }
    if (cfg_score <= 0) {
        msg_Err(va, "Failed to find a supported decoder configuration");
        return VLC_EGENERIC;
    }

    /* Create the decoder */
    ID3D11VideoDecoder *decoder;
    hr = ID3D11VideoDevice_CreateVideoDecoder( (ID3D11VideoDevice*) dx_sys->d3ddec, &decoderDesc, &sys->cfg, &decoder );
    if (FAILED(hr)) {
        msg_Err(va, "ID3D11VideoDevice_CreateVideoDecoder failed. (hr=0x%lX)", hr);
        dx_sys->decoder = NULL;
        return VLC_EGENERIC;
    }
    dx_sys->decoder = (IUnknown*) decoder;

    if (IsEqualGUID(&dx_sys->input, &DXVA_Intel_H264_NoFGT_ClearVideo))
        sys->hw.workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;

    msg_Dbg(va, "DxCreateVideoDecoder succeed");
    return VLC_SUCCESS;
}

static void DxDestroySurfaces(vlc_va_t *va)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    if (dx_sys->surface_count) {
        ID3D11Resource *p_texture;
        ID3D11VideoDecoderOutputView_GetResource( (ID3D11VideoDecoderOutputView*) dx_sys->hw_surface[0], &p_texture );
        ID3D11Resource_Release(p_texture);
        ID3D11Resource_Release(p_texture);
    }

    if (va->sys->staging)
        ID3D11Resource_Release(va->sys->staging);
#if DEBUG_LEAK
    va->sys->staging = NULL;
#endif
}

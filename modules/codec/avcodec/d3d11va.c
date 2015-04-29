/*****************************************************************************
 * dxva2.c: Video Acceleration helpers
 *****************************************************************************
 * Copyright (C) 2009 Geoffroy Couprie
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Geoffroy Couprie <geal@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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


# if _WIN32_WINNT < 0x600
/* dxva2 needs Vista support */
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x600
# endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_fourcc.h>
#include <vlc_plugin.h>
#include <vlc_codecs.h>

#include <libavcodec/avcodec.h>
#    define COBJMACROS
#    include <libavcodec/d3d11va.h>

#include "avcodec.h"
#include "va.h"
#include "../../packetizer/h264_nal.h"

static int Open(vlc_va_t *, AVCodecContext *, enum PixelFormat,
                const es_format_t *, picture_sys_t *p_sys);
static void Close(vlc_va_t *, AVCodecContext *);

vlc_module_begin()
    set_description(N_("Direct3D11 Video Acceleration"))
    set_capability("hw decoder", 0)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_callbacks(Open, Close)
vlc_module_end()

#include <windows.h>
#include <windowsx.h>
#include <ole2.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <d3d11.h>

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

MS_GUID(IID_IDirectXVideoDecoderService, 0xfc51a551, 0xd5e7, 0x11d9, 0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);
MS_GUID(IID_IDirectXVideoAccelerationService, 0xfc51a550, 0xd5e7, 0x11d9, 0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);

MS_GUID    (DXVA_NoEncrypt,                         0x1b81bed0, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);

/* */
typedef struct {
    const char   *name;
    const GUID   *guid;
    int          codec;
    const int    *p_profiles; // NULL or ends with 0
} dxva2_mode_t;

static const int PROF_MPEG2_SIMPLE[] = { FF_PROFILE_MPEG2_SIMPLE, 0 };
static const int PROF_MPEG2_MAIN[]   = { FF_PROFILE_MPEG2_SIMPLE,
                                         FF_PROFILE_MPEG2_MAIN, 0 };
static const int PROF_H264_HIGH[]    = { FF_PROFILE_H264_CONSTRAINED_BASELINE,
                                         FF_PROFILE_H264_MAIN,
                                         FF_PROFILE_H264_HIGH, 0 };
static const int PROF_HEVC_MAIN[]    = { FF_PROFILE_HEVC_MAIN, 0 };
static const int PROF_HEVC_MAIN10[]  = { FF_PROFILE_HEVC_MAIN,
                                         FF_PROFILE_HEVC_MAIN_10, 0 };

/* XXX Prefered modes must come first */
static const dxva2_mode_t dxva2_modes[] = {
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

static const dxva2_mode_t *Dxva2FindMode(const GUID *guid)
{
    for (unsigned i = 0; dxva2_modes[i].name; i++) {
        if (IsEqualGUID(dxva2_modes[i].guid, guid))
            return &dxva2_modes[i];
    }
    return NULL;
}

/* */
typedef struct {
    const char   *name;
    DXGI_FORMAT    format;
    vlc_fourcc_t codec;
} d3d_format_t;
/* XXX Prefered format must come first */
static const d3d_format_t d3d_formats[] = {
    { "YV12",   MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_YV12 },
    { "NV12",   MAKEFOURCC('N','V','1','2'),    VLC_CODEC_NV12 },
    { "IMC3",   MAKEFOURCC('I','M','C','3'),    VLC_CODEC_YV12 },

    { NULL, 0, 0 }
};

static const d3d_format_t *D3dFindFormat(DXGI_FORMAT format)
{
    for (unsigned i = 0; d3d_formats[i].name; i++) {
        if (d3d_formats[i].format == format)
            return &d3d_formats[i];
    }
    return NULL;
}

/* */
typedef struct {
    LPDIRECT3DSURFACE9 d3d;
    int                refcount;
    unsigned int       order;
    vlc_mutex_t        *p_lock;
} vlc_va_surface_t;

#define VA_DXVA2_MAX_SURFACE_COUNT (64)
struct vlc_va_sys_t
{
    int          codec_id;
    int          width;
    int          height;

    vlc_mutex_t     surface_lock;

    /* DLL */
    HINSTANCE             hd3d9_dll;
    HINSTANCE             hdxva2_dll;

    /* Direct3D */
    LPDIRECT3D9            d3dobj;
    D3DADAPTER_IDENTIFIER9 d3dai;
    LPDIRECT3DDEVICE9      d3ddev;

    /* Device manager */
    UINT                     token;
    IDirect3DDeviceManager9  *devmng;
    HANDLE                   device;

    /* Video service */
    IDirectXVideoDecoderService  *vs;
    GUID                         input;
    DXGI_FORMAT                    render;

    /* Video decoder */
    DXVA2_ConfigPictureDecode    cfg;
    IDirectXVideoDecoder         *decoder;

    /* */
    struct dxva_context hw;

    /* */
    unsigned     surface_count;
    unsigned     surface_order;
    int          surface_width;
    int          surface_height;
    vlc_fourcc_t surface_chroma;

    int          thread_count;

    vlc_va_surface_t surface[VA_DXVA2_MAX_SURFACE_COUNT];
    LPDIRECT3DSURFACE9 hw_surface[VA_DXVA2_MAX_SURFACE_COUNT];
};

struct picture_sys_t
{
    LPDIRECT3DSURFACE9 surface;
};

/* */
static int D3dCreateDevice(vlc_va_t *);
static void D3dDestroyDevice(vlc_va_sys_t *);
static char *DxDescribe(vlc_va_sys_t *);

static int D3dCreateDeviceManager(vlc_va_t *);
static void D3dDestroyDeviceManager(vlc_va_sys_t *);

static int DxCreateVideoService(vlc_va_t *);
static void DxDestroyVideoService(vlc_va_sys_t *);
static int DxFindVideoServiceConversion(vlc_va_t *, GUID *input, DXGI_FORMAT *output, const es_format_t *fmt);

static int DxCreateVideoDecoder(vlc_va_t *,
                                int codec_id, const video_format_t *, bool);
static void DxDestroyVideoDecoder(vlc_va_sys_t *);
static int DxResetVideoDecoder(vlc_va_t *);

static bool profile_supported(const dxva2_mode_t *mode, const es_format_t *fmt);

/* */
static int Setup(vlc_va_t *va, AVCodecContext *avctx, vlc_fourcc_t *chroma)
{
    vlc_va_sys_t *sys = va->sys;

    if (sys->width == avctx->coded_width && sys->height == avctx->coded_height
     && sys->decoder != NULL)
        goto ok;

    /* */
    DxDestroyVideoDecoder(sys);

    avctx->hwaccel_context = NULL;
    *chroma = 0;
    if (avctx->coded_width <= 0 || avctx->coded_height <= 0)
        return VLC_EGENERIC;

    /* FIXME transmit a video_format_t by VaSetup directly */
    video_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.i_width = avctx->coded_width;
    fmt.i_height = avctx->coded_height;

    if (DxCreateVideoDecoder(va, sys->codec_id, &fmt, avctx->active_thread_type & FF_THREAD_FRAME))
        return VLC_EGENERIC;
    /* */
    sys->hw.decoder = sys->decoder;
    sys->hw.cfg = &sys->cfg;
    sys->hw.surface_count = sys->surface_count;
    sys->hw.surface = sys->hw_surface;

    /* */
ok:
    avctx->hwaccel_context = &sys->hw;
    *chroma = VLC_CODEC_D3D9_OPAQUE;

    return VLC_SUCCESS;
}


static int Extract(vlc_va_t *va, picture_t *picture, uint8_t *data)
{
    vlc_va_sys_t *sys = va->sys;
    LPDIRECT3DSURFACE9 d3d = (LPDIRECT3DSURFACE9)(uintptr_t)data;
    picture_sys_t *p_sys = picture->p_sys;
    LPDIRECT3DSURFACE9 output = p_sys->surface;

    assert(d3d != output);
#ifndef NDEBUG
    LPDIRECT3DDEVICE9 srcDevice, dstDevice;
    IDirect3DSurface9_GetDevice(d3d, &srcDevice);
    IDirect3DSurface9_GetDevice(output, &dstDevice);
    assert(srcDevice == dstDevice);
#endif

    HRESULT hr;
    RECT visibleSource;
    visibleSource.left = 0;
    visibleSource.top = 0;
    visibleSource.right = picture->format.i_visible_width;
    visibleSource.bottom = picture->format.i_visible_height;
    hr = IDirect3DDevice9_StretchRect( sys->d3ddev, d3d, &visibleSource, output, &visibleSource, D3DTEXF_NONE);
    if (FAILED(hr)) {
        msg_Err(va, "Failed to copy the hw surface to the decoder surface (hr=0x%0lx)", hr );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/* FIXME it is nearly common with VAAPI */
static int Get(vlc_va_t *va, picture_t *pic, uint8_t **data)
{
    vlc_va_sys_t *sys = va->sys;

    /* Check the device */
    HRESULT hr = IDirect3DDeviceManager9_TestDevice(sys->devmng, sys->device);
    if (hr == DXVA2_E_NEW_VIDEO_DEVICE) {
        if (DxResetVideoDecoder(va))
            return VLC_EGENERIC;
    } else if (FAILED(hr)) {
        msg_Err(va, "IDirect3DDeviceManager9_TestDevice %u", (unsigned)hr);
        return VLC_EGENERIC;
    }

    vlc_mutex_lock( &sys->surface_lock );

    /* Grab an unused surface, in case none are, try the oldest
     * XXX using the oldest is a workaround in case a problem happens with libavcodec */
    unsigned i, old;
    for (i = 0, old = 0; i < sys->surface_count; i++) {
        vlc_va_surface_t *surface = &sys->surface[i];

        if (!surface->refcount)
            break;

        if (surface->order < sys->surface[old].order)
            old = i;
    }
    if (i >= sys->surface_count)
        i = old;

    vlc_va_surface_t *surface = &sys->surface[i];

    surface->refcount = 1;
    surface->order = sys->surface_order++;
    *data = (void *)surface->d3d;
    pic->context = surface;

    vlc_mutex_unlock( &sys->surface_lock );

    return VLC_SUCCESS;
}

static void Release(void *opaque, uint8_t *data)
{
    picture_t *pic = opaque;
    vlc_va_surface_t *surface = pic->context;
    vlc_mutex_lock( surface->p_lock );

    surface->refcount--;
    pic->context = NULL;
    picture_Release(pic);
    (void) data;

    vlc_mutex_unlock( surface->p_lock );
}

static void Close(vlc_va_t *va, AVCodecContext *ctx)
{
    vlc_va_sys_t *sys = va->sys;

    (void) ctx;
    DxDestroyVideoDecoder(sys);
    DxDestroyVideoService(sys);
    D3dDestroyDeviceManager(sys);
    D3dDestroyDevice(sys);

    if (sys->hdxva2_dll)
        FreeLibrary(sys->hdxva2_dll);
    if (sys->hd3d9_dll)
        FreeLibrary(sys->hd3d9_dll);
    vlc_mutex_destroy( &sys->surface_lock );

    free((char *)va->description);
    free(sys);
}

static int Open(vlc_va_t *va, AVCodecContext *ctx, enum PixelFormat pix_fmt,
                const es_format_t *fmt, picture_sys_t *p_sys)
{
    if (pix_fmt != AV_PIX_FMT_DXVA2_VLD)
        return VLC_EGENERIC;

    (void) p_sys;

    vlc_va_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    va->sys = sys;
    sys->codec_id = ctx->codec_id;

    vlc_mutex_init( &sys->surface_lock );

    /* Load dll*/
    sys->hd3d9_dll = LoadLibrary(TEXT("D3D9.DLL"));
    if (!sys->hd3d9_dll) {
        msg_Warn(va, "cannot load d3d9.dll");
        goto error;
    }
    sys->hdxva2_dll = LoadLibrary(TEXT("DXVA2.DLL"));
    if (!sys->hdxva2_dll) {
        msg_Warn(va, "cannot load dxva2.dll");
        goto error;
    }
    msg_Dbg(va, "DLLs loaded");

    sys->d3ddev = NULL;
    if (p_sys!=NULL)
        IDirect3DSurface9_GetDevice(p_sys->surface, &sys->d3ddev);

    if (sys->d3ddev) {
        msg_Dbg(va, "Reusing D3D9 device");
    } else {
        /* */
        if (D3dCreateDevice(va)) {
            msg_Err(va, "Failed to create Direct3D device");
            goto error;
        }
        msg_Dbg(va, "D3dCreateDevice succeed");
    }

    if (D3dCreateDeviceManager(va)) {
        msg_Err(va, "D3dCreateDeviceManager failed");
        goto error;
    }

    if (DxCreateVideoService(va)) {
        msg_Err(va, "DxCreateVideoService failed");
        goto error;
    }

    /* */
    if (DxFindVideoServiceConversion(va, &sys->input, &sys->render, fmt)) {
        msg_Err(va, "DxFindVideoServiceConversion failed");
        goto error;
    }

    sys->thread_count = ctx->thread_count;

    /* TODO print the hardware name/vendor for debugging purposes */
    va->description = DxDescribe(sys);
    va->setup   = Setup;
    va->get     = Get;
    va->release = Release;
    va->extract = Extract;
    return VLC_SUCCESS;

error:
    Close(va, ctx);
    return VLC_EGENERIC;
}
/* */

/**
 * It creates a Direct3D device usable for DXVA 2
 */
static int D3dCreateDevice(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    /* */
    LPDIRECT3D9 (WINAPI *Create9)(UINT SDKVersion);
    Create9 = (void *)GetProcAddress(sys->hd3d9_dll, "Direct3DCreate9");
    if (!Create9) {
        msg_Err(va, "Cannot locate reference to Direct3DCreate9 ABI in DLL");
        return VLC_EGENERIC;
    }

    /* */
    LPDIRECT3D9 d3dobj;
    d3dobj = Create9(D3D_SDK_VERSION);
    if (!d3dobj) {
        msg_Err(va, "Direct3DCreate9 failed");
        return VLC_EGENERIC;
    }
    sys->d3dobj = d3dobj;

    /* */
    D3DADAPTER_IDENTIFIER9 *d3dai = &sys->d3dai;
    if (FAILED(IDirect3D9_GetAdapterIdentifier(sys->d3dobj,
                                               D3DADAPTER_DEFAULT, 0, d3dai))) {
        msg_Warn(va, "IDirect3D9_GetAdapterIdentifier failed");
        ZeroMemory(d3dai, sizeof(*d3dai));
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
    if (FAILED(IDirect3D9_CreateDevice(d3dobj, D3DADAPTER_DEFAULT,
                                       D3DDEVTYPE_HAL, GetDesktopWindow(),
                                       D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                                       D3DCREATE_MULTITHREADED,
                                       &d3dpp, &d3ddev))) {
        msg_Err(va, "IDirect3D9_CreateDevice failed");
        return VLC_EGENERIC;
    }
    sys->d3ddev = d3ddev;

    return VLC_SUCCESS;
}

/**
 * It releases a Direct3D device and its resources.
 */
static void D3dDestroyDevice(vlc_va_sys_t *va)
{
    if (va->d3ddev)
        IDirect3DDevice9_Release(va->d3ddev);
    if (va->d3dobj)
        IDirect3D9_Release(va->d3dobj);
}
/**
 * It describes our Direct3D object
 */
static char *DxDescribe(vlc_va_sys_t *va)
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
    D3DADAPTER_IDENTIFIER9 *id = &va->d3dai;

    const char *vendor = "Unknown";
    for (int i = 0; vendors[i].id != 0; i++) {
        if (vendors[i].id == id->VendorId) {
            vendor = vendors[i].name;
            break;
        }
    }

    char *description;
    if (asprintf(&description, "DXVA2 (%.*s, vendor %lu(%s), device %lu, revision %lu)",
                 sizeof(id->Description), id->Description,
                 id->VendorId, vendor, id->DeviceId, id->Revision) < 0)
        return NULL;
    return description;
}

/**
 * It creates a Direct3D device manager
 */
static int D3dCreateDeviceManager(vlc_va_t *va)
{
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

    HRESULT hr = IDirect3DDeviceManager9_ResetDevice(devmng, sys->d3ddev, token);
    if (FAILED(hr)) {
        msg_Err(va, "IDirect3DDeviceManager9_ResetDevice failed: %08x", (unsigned)hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
/**
 * It destroys a Direct3D device manager
 */
static void D3dDestroyDeviceManager(vlc_va_sys_t *va)
{
    if (va->devmng)
        IDirect3DDeviceManager9_Release(va->devmng);
}

/**
 * It creates a DirectX video service
 */
static int DxCreateVideoService(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    HRESULT (WINAPI *CreateVideoService)(IDirect3DDevice9 *,
                                         REFIID riid,
                                         void **ppService);
    CreateVideoService =
      (void *)GetProcAddress(sys->hdxva2_dll, "DXVA2CreateVideoService");

    if (!CreateVideoService) {
        msg_Err(va, "cannot load function");
        return 4;
    }
    msg_Info(va, "DXVA2CreateVideoService Success!");

    HRESULT hr;

    HANDLE device;
    hr = IDirect3DDeviceManager9_OpenDeviceHandle(sys->devmng, &device);
    if (FAILED(hr)) {
        msg_Err(va, "OpenDeviceHandle failed");
        return VLC_EGENERIC;
    }
    sys->device = device;

    void *pv;
    hr = IDirect3DDeviceManager9_GetVideoService(sys->devmng, device,
                                        &IID_IDirectXVideoDecoderService, &pv);
    if (FAILED(hr)) {
        msg_Err(va, "GetVideoService failed");
        return VLC_EGENERIC;
    }
    sys->vs = pv;

    return VLC_SUCCESS;
}

/**
 * It destroys a DirectX video service
 */
static void DxDestroyVideoService(vlc_va_sys_t *va)
{
    if (va->device)
        IDirect3DDeviceManager9_CloseDeviceHandle(va->devmng, va->device);
    if (va->vs)
        IDirectXVideoDecoderService_Release(va->vs);
}

static bool profile_supported(const dxva2_mode_t *mode, const es_format_t *fmt)
{
    bool is_supported = mode->p_profiles == NULL || !mode->p_profiles[0];
    if (!is_supported)
    {
        int profile = fmt->i_profile;
        if (mode->codec == AV_CODEC_ID_H264)
        {
            size_t h264_profile;
            if ( h264_get_profile_level(fmt, &h264_profile, NULL, NULL) )
                profile = h264_profile;
        }

        if (profile <= 0)
            is_supported = true;
        else for (const int *p_profile = &mode->p_profiles[0]; *p_profile; ++p_profile)
        {
            if (*p_profile == profile)
            {
                is_supported = true;
                break;
            }
        }
    }
    return is_supported;
}

/**
 * Find the best suited decoder mode GUID and render format.
 */
static int DxFindVideoServiceConversion(vlc_va_t *va, GUID *input, DXGI_FORMAT *output, const es_format_t *fmt)
{
    vlc_va_sys_t *sys = va->sys;

    /* Retreive supported modes from the decoder service */
    UINT input_count = 0;
    GUID *input_list = NULL;
    if (FAILED(IDirectXVideoDecoderService_GetDecoderDeviceGuids(sys->vs,
                                                                 &input_count,
                                                                 &input_list))) {
        msg_Err(va, "IDirectXVideoDecoderService_GetDecoderDeviceGuids failed");
        return VLC_EGENERIC;
    }
    for (unsigned i = 0; i < input_count; i++) {
        const GUID *g = &input_list[i];
        const dxva2_mode_t *mode = Dxva2FindMode(g);
        if (mode) {
            msg_Dbg(va, "- '%s' is supported by hardware", mode->name);
        } else {
            msg_Warn(va, "- Unknown GUID = " GUID_FMT, GUID_PRINT( *g ) );
        }
    }

    /* Try all supported mode by our priority */
    for (unsigned i = 0; dxva2_modes[i].name; i++) {
        const dxva2_mode_t *mode = &dxva2_modes[i];
        if (!mode->codec || mode->codec != sys->codec_id)
            continue;

        /* */
        bool is_supported = false;
        for (const GUID *g = &input_list[0]; !is_supported && g < &input_list[input_count]; g++) {
            is_supported = IsEqualGUID(mode->guid, g);
        }
        if ( is_supported )
        {
            is_supported = profile_supported( mode, fmt );
            if (!is_supported)
                msg_Warn( va, "Unsupported profile for DXVA2 HWAccel: %d", fmt->i_profile );
        }
        if (!is_supported)
            continue;

        /* */
        msg_Dbg(va, "Trying to use '%s' as input", mode->name);
        UINT      output_count = 0;
        DXGI_FORMAT *output_list = NULL;
        if (FAILED(IDirectXVideoDecoderService_GetDecoderRenderTargets(sys->vs, mode->guid,
                                                                       &output_count,
                                                                       &output_list))) {
            msg_Err(va, "IDirectXVideoDecoderService_GetDecoderRenderTargets failed");
            continue;
        }
        for (unsigned j = 0; j < output_count; j++) {
            const DXGI_FORMAT f = output_list[j];
            const d3d_format_t *format = D3dFindFormat(f);
            if (format) {
                msg_Dbg(va, "%s is supported for output", format->name);
            } else {
                msg_Dbg(va, "%d is supported for output (%4.4s)", f, (const char*)&f);
            }
        }

        /* */
        for (unsigned j = 0; d3d_formats[j].name; j++) {
            const d3d_format_t *format = &d3d_formats[j];

            /* */
            bool is_supported = false;
            for (unsigned k = 0; !is_supported && k < output_count; k++) {
                is_supported = format->format == output_list[k];
            }
            if (!is_supported)
                continue;

            /* We have our solution */
            msg_Dbg(va, "Using '%s' to decode to '%s'", mode->name, format->name);
            *input  = *mode->guid;
            *output = format->format;
            CoTaskMemFree(output_list);
            CoTaskMemFree(input_list);
            return VLC_SUCCESS;
        }
        CoTaskMemFree(output_list);
    }
    CoTaskMemFree(input_list);
    return VLC_EGENERIC;
}

/**
 * It creates a DXVA2 decoder using the given video format
 */
static int DxCreateVideoDecoder(vlc_va_t *va, int codec_id,
                                const video_format_t *fmt, bool b_threading)
{
    vlc_va_sys_t *sys = va->sys;
    int surface_alignment = 16;
    int surface_count = 4;

    /* */
    msg_Dbg(va, "DxCreateVideoDecoder id %d %dx%d",
            codec_id, fmt->i_width, fmt->i_height);

    sys->width  = fmt->i_width;
    sys->height = fmt->i_height;

    switch ( codec_id )
    {
    case AV_CODEC_ID_MPEG2VIDEO:
        /* decoding MPEG-2 requires additional alignment on some Intel GPUs,
           but it causes issues for H.264 on certain AMD GPUs..... */
        surface_alignment = 32;
        surface_count += 2;
        break;
    case AV_CODEC_ID_HEVC:
        /* the HEVC DXVA2 spec asks for 128 pixel aligned surfaces to ensure
           all coding features have enough room to work with */
        surface_alignment = 128;
        surface_count += 16;
        break;
    case AV_CODEC_ID_H264:
        surface_count += 16;
        break;
    default:
        surface_count += 2;
    }

#define ALIGN(x, y) (((x) + ((y) - 1)) & ~((y) - 1))
    sys->surface_width  = ALIGN(fmt->i_width, surface_alignment);
    sys->surface_height = ALIGN(fmt->i_height, surface_alignment);

    if ( b_threading )
        surface_count += sys->thread_count;

    if (surface_count > VA_DXVA2_MAX_SURFACE_COUNT)
        return VLC_EGENERIC;
    sys->surface_count = surface_count;
    if (FAILED(IDirectXVideoDecoderService_CreateSurface(sys->vs,
                                                         sys->surface_width,
                                                         sys->surface_height,
                                                         sys->surface_count - 1,
                                                         sys->render,
                                                         D3DPOOL_DEFAULT,
                                                         0,
                                                         DXVA2_VideoDecoderRenderTarget,
                                                         sys->hw_surface,
                                                         NULL))) {
        msg_Err(va, "IDirectXVideoAccelerationService_CreateSurface failed");
        sys->surface_count = 0;
        return VLC_EGENERIC;
    }
    for (unsigned i = 0; i < sys->surface_count; i++) {
        vlc_va_surface_t *surface = &sys->surface[i];
        surface->d3d = sys->hw_surface[i];
        surface->refcount = 0;
        surface->order = 0;
        surface->p_lock = &sys->surface_lock;
    }
    msg_Dbg(va, "IDirectXVideoAccelerationService_CreateSurface succeed with %d surfaces (%dx%d)",
            sys->surface_count, fmt->i_width, fmt->i_height);

    /* */
    DXVA2_VideoDesc dsc;
    ZeroMemory(&dsc, sizeof(dsc));
    dsc.SampleWidth     = fmt->i_width;
    dsc.SampleHeight    = fmt->i_height;
    dsc.Format          = sys->render;
    if (fmt->i_frame_rate > 0 && fmt->i_frame_rate_base > 0) {
        dsc.InputSampleFreq.Numerator   = fmt->i_frame_rate;
        dsc.InputSampleFreq.Denominator = fmt->i_frame_rate_base;
    } else {
        dsc.InputSampleFreq.Numerator   = 0;
        dsc.InputSampleFreq.Denominator = 0;
    }
    dsc.OutputFrameFreq = dsc.InputSampleFreq;
    dsc.UABProtectionLevel = FALSE;
    dsc.Reserved = 0;

    /* FIXME I am unsure we can let unknown everywhere */
    DXVA2_ExtendedFormat *ext = &dsc.SampleFormat;
    ext->SampleFormat = 0;//DXVA2_SampleUnknown;
    ext->VideoChromaSubsampling = 0;//DXVA2_VideoChromaSubsampling_Unknown;
    ext->NominalRange = 0;//DXVA2_NominalRange_Unknown;
    ext->VideoTransferMatrix = 0;//DXVA2_VideoTransferMatrix_Unknown;
    ext->VideoLighting = 0;//DXVA2_VideoLighting_Unknown;
    ext->VideoPrimaries = 0;//DXVA2_VideoPrimaries_Unknown;
    ext->VideoTransferFunction = 0;//DXVA2_VideoTransFunc_Unknown;

    /* List all configurations available for the decoder */
    UINT                      cfg_count = 0;
    DXVA2_ConfigPictureDecode *cfg_list = NULL;
    if (FAILED(IDirectXVideoDecoderService_GetDecoderConfigurations(sys->vs,
                                                                    &sys->input,
                                                                    &dsc,
                                                                    NULL,
                                                                    &cfg_count,
                                                                    &cfg_list))) {
        msg_Err(va, "IDirectXVideoDecoderService_GetDecoderConfigurations failed");
        return VLC_EGENERIC;
    }
    msg_Dbg(va, "we got %d decoder configurations", cfg_count);

    /* Select the best decoder configuration */
    int cfg_score = 0;
    for (unsigned i = 0; i < cfg_count; i++) {
        const DXVA2_ConfigPictureDecode *cfg = &cfg_list[i];

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
    CoTaskMemFree(cfg_list);
    if (cfg_score <= 0) {
        msg_Err(va, "Failed to find a supported decoder configuration");
        return VLC_EGENERIC;
    }

    /* Create the decoder */
    IDirectXVideoDecoder *decoder;
    if (FAILED(IDirectXVideoDecoderService_CreateVideoDecoder(sys->vs,
                                                              &sys->input,
                                                              &dsc,
                                                              &sys->cfg,
                                                              sys->hw_surface,
                                                              sys->surface_count,
                                                              &decoder))) {
        msg_Err(va, "IDirectXVideoDecoderService_CreateVideoDecoder failed");
        return VLC_EGENERIC;
    }
    sys->decoder = decoder;

    if (IsEqualGUID(&sys->input, &DXVADDI_Intel_ModeH264_E))
        sys->hw.workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;

    msg_Dbg(va, "IDirectXVideoDecoderService_CreateVideoDecoder succeed");
    return VLC_SUCCESS;
}

static void DxDestroyVideoDecoder(vlc_va_sys_t *va)
{
    if (va->decoder)
        IDirectXVideoDecoder_Release(va->decoder);
    va->decoder = NULL;

    for (unsigned i = 0; i < va->surface_count; i++)
        IDirect3DSurface9_Release(va->surface[i].d3d);
    va->surface_count = 0;
}

static int DxResetVideoDecoder(vlc_va_t *va)
{
    msg_Err(va, "DxResetVideoDecoder unimplemented");
    return VLC_EGENERIC;
}


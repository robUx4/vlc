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

#define DEBUG_LEAK 0
#define D3D11_DR 0

# if _WIN32_WINNT < 0x600
/* d3d11 needs Vista support */
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x600
# endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_fourcc.h>
#include <vlc_plugin.h>
#include <vlc_codecs.h>
#include <vlc_charset.h>

#include <libavcodec/avcodec.h>
#    define COBJMACROS
#    include <libavcodec/d3d11va.h>

#include "avcodec.h"
#include "va.h"
#include "../../packetizer/h264_nal.h"
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

#include <windows.h>
#include <windowsx.h>
#include <ole2.h>
#include <commctrl.h>
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
#include <d3d10.h>

#endif /* __MINGW32__ */

MS_GUID    (DXVA_NoEncrypt,                         0x1b81bed0, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);

DEFINE_GUID(IID_ID3D11VideoDevice,   0x10EC4D5B, 0x975A, 0x4689, 0xB9, 0xE4, 0xD0, 0xAA, 0xC3, 0x0F, 0xE3, 0x33);
DEFINE_GUID(IID_ID3D11VideoContext,  0x61F21C45, 0x3C0E, 0x4a74, 0x9C, 0xEA, 0x67, 0x10, 0x0D, 0x9A, 0xD5, 0xE4);
DEFINE_GUID(IID_IDXGIDevice,         0x54ec77fa, 0x1377, 0x44e6, 0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c);
DEFINE_GUID(IID_ID3D10Multithread,   0x9b7e4e00, 0x342c, 0x4106, 0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0);
#ifndef NDEBUG
#ifndef __IDXGIDebug_FWD_DEFINED__
#define __IDXGIDebug_FWD_DEFINED__
typedef interface IDXGIDebug IDXGIDebug;

#endif 	/* __IDXGIDebug_FWD_DEFINED__ */

DEFINE_GUID(DXGI_DEBUG_ALL, 0xe48ae283, 0xda80, 0x490b, 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8);
DEFINE_GUID(DXGI_DEBUG_DX, 0x35cdd7fc, 0x13b2, 0x421d, 0xa5, 0xd7, 0x7e, 0x44, 0x51, 0x28, 0x7d, 0x64);
DEFINE_GUID(DXGI_DEBUG_DXGI, 0x25cddaa4, 0xb1c6, 0x47e1, 0xac, 0x3e, 0x98, 0x87, 0x5b, 0x5a, 0x2e, 0x2a);
DEFINE_GUID(DXGI_DEBUG_APP, 0x6cd6e01, 0x4219, 0x4ebd, 0x87, 0x9, 0x27, 0xed, 0x23, 0x36, 0xc, 0x62);

typedef
enum DXGI_DEBUG_RLO_FLAGS
    {
        DXGI_DEBUG_RLO_SUMMARY	= 0x1,
        DXGI_DEBUG_RLO_DETAIL	= 0x2,
        DXGI_DEBUG_RLO_ALL	= 0x3
    } 	DXGI_DEBUG_RLO_FLAGS;

#ifndef __IDXGIDebug_INTERFACE_DEFINED__
#define __IDXGIDebug_INTERFACE_DEFINED__

/* interface IDXGIDebug */
/* [unique][local][object][uuid] */


EXTERN_C const IID IID_IDXGIDebug;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("119E7452-DE9E-40fe-8806-88F90C12B441")
    IDXGIDebug : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE ReportLiveObjects(
            GUID apiid,
            DXGI_DEBUG_RLO_FLAGS flags) = 0;

    };


#else 	/* C style interface */

    typedef struct IDXGIDebugVtbl
    {
        BEGIN_INTERFACE

        HRESULT ( STDMETHODCALLTYPE *QueryInterface )(
            IDXGIDebug * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */
            void **ppvObject);

        ULONG ( STDMETHODCALLTYPE *AddRef )(
            IDXGIDebug * This);

        ULONG ( STDMETHODCALLTYPE *Release )(
            IDXGIDebug * This);

        HRESULT ( STDMETHODCALLTYPE *ReportLiveObjects )(
            IDXGIDebug * This,
            GUID apiid,
            DXGI_DEBUG_RLO_FLAGS flags);

        END_INTERFACE
    } IDXGIDebugVtbl;

    interface IDXGIDebug
    {
        CONST_VTBL struct IDXGIDebugVtbl *lpVtbl;
    };



#ifdef COBJMACROS


#define IDXGIDebug_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) )

#define IDXGIDebug_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) )

#define IDXGIDebug_Release(This)	\
    ( (This)->lpVtbl -> Release(This) )


#define IDXGIDebug_ReportLiveObjects(This,apiid,flags)	\
    ( (This)->lpVtbl -> ReportLiveObjects(This,apiid,flags) )

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IDXGIDebug_INTERFACE_DEFINED__ */

DEFINE_GUID(IID_IDXGIDebug,0x119E7452,0xDE9E,0x40fe,0x88,0x06,0x88,0xF9,0x0C,0x12,0xB4,0x41);

#ifndef __ID3D11Debug_FWD_DEFINED__
#define __ID3D11Debug_FWD_DEFINED__
typedef interface ID3D11Debug ID3D11Debug;

#endif 	/* __ID3D11Debug_FWD_DEFINED__ */

typedef
enum D3D11_RLDO_FLAGS
    {
        D3D11_RLDO_SUMMARY	= 0x1,
        D3D11_RLDO_DETAIL	= 0x2
    } 	D3D11_RLDO_FLAGS;

#ifndef __ID3D11Debug_INTERFACE_DEFINED__
#define __ID3D11Debug_INTERFACE_DEFINED__

/* interface ID3D11Debug */
/* [unique][local][object][uuid] */


#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("79cf2233-7536-4948-9d36-1e4692dc5760")
    ID3D11Debug : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE SetFeatureMask(
            UINT Mask) = 0;

        virtual UINT STDMETHODCALLTYPE GetFeatureMask( void) = 0;

        virtual HRESULT STDMETHODCALLTYPE SetPresentPerRenderOpDelay(
            UINT Milliseconds) = 0;

        virtual UINT STDMETHODCALLTYPE GetPresentPerRenderOpDelay( void) = 0;

        virtual HRESULT STDMETHODCALLTYPE SetSwapChain(
            /* [annotation] */
            _In_opt_  IDXGISwapChain *pSwapChain) = 0;

        virtual HRESULT STDMETHODCALLTYPE GetSwapChain(
            /* [annotation] */
            _Out_  IDXGISwapChain **ppSwapChain) = 0;

        virtual HRESULT STDMETHODCALLTYPE ValidateContext(
            /* [annotation] */
            _In_  ID3D11DeviceContext *pContext) = 0;

        virtual HRESULT STDMETHODCALLTYPE ReportLiveDeviceObjects(
            D3D11_RLDO_FLAGS Flags) = 0;

        virtual HRESULT STDMETHODCALLTYPE ValidateContextForDispatch(
            /* [annotation] */
            _In_  ID3D11DeviceContext *pContext) = 0;

    };


#else 	/* C style interface */

    typedef struct ID3D11DebugVtbl
    {
        BEGIN_INTERFACE

        HRESULT ( STDMETHODCALLTYPE *QueryInterface )(
            ID3D11Debug * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */
            void **ppvObject);

        ULONG ( STDMETHODCALLTYPE *AddRef )(
            ID3D11Debug * This);

        ULONG ( STDMETHODCALLTYPE *Release )(
            ID3D11Debug * This);

        HRESULT ( STDMETHODCALLTYPE *SetFeatureMask )(
            ID3D11Debug * This,
            UINT Mask);

        UINT ( STDMETHODCALLTYPE *GetFeatureMask )(
            ID3D11Debug * This);

        HRESULT ( STDMETHODCALLTYPE *SetPresentPerRenderOpDelay )(
            ID3D11Debug * This,
            UINT Milliseconds);

        UINT ( STDMETHODCALLTYPE *GetPresentPerRenderOpDelay )(
            ID3D11Debug * This);

        HRESULT ( STDMETHODCALLTYPE *SetSwapChain )(
            ID3D11Debug * This,
            /* [annotation] */
            _In_opt_  IDXGISwapChain *pSwapChain);

        HRESULT ( STDMETHODCALLTYPE *GetSwapChain )(
            ID3D11Debug * This,
            /* [annotation] */
            _Out_  IDXGISwapChain **ppSwapChain);

        HRESULT ( STDMETHODCALLTYPE *ValidateContext )(
            ID3D11Debug * This,
            /* [annotation] */
            _In_  ID3D11DeviceContext *pContext);

        HRESULT ( STDMETHODCALLTYPE *ReportLiveDeviceObjects )(
            ID3D11Debug * This,
            D3D11_RLDO_FLAGS Flags);

        HRESULT ( STDMETHODCALLTYPE *ValidateContextForDispatch )(
            ID3D11Debug * This,
            /* [annotation] */
            _In_  ID3D11DeviceContext *pContext);

        END_INTERFACE
    } ID3D11DebugVtbl;

    interface ID3D11Debug
    {
        CONST_VTBL struct ID3D11DebugVtbl *lpVtbl;
    };



#ifdef COBJMACROS


#define ID3D11Debug_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) )

#define ID3D11Debug_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) )

#define ID3D11Debug_Release(This)	\
    ( (This)->lpVtbl -> Release(This) )


#define ID3D11Debug_SetFeatureMask(This,Mask)	\
    ( (This)->lpVtbl -> SetFeatureMask(This,Mask) )

#define ID3D11Debug_GetFeatureMask(This)	\
    ( (This)->lpVtbl -> GetFeatureMask(This) )

#define ID3D11Debug_SetPresentPerRenderOpDelay(This,Milliseconds)	\
    ( (This)->lpVtbl -> SetPresentPerRenderOpDelay(This,Milliseconds) )

#define ID3D11Debug_GetPresentPerRenderOpDelay(This)	\
    ( (This)->lpVtbl -> GetPresentPerRenderOpDelay(This) )

#define ID3D11Debug_SetSwapChain(This,pSwapChain)	\
    ( (This)->lpVtbl -> SetSwapChain(This,pSwapChain) )

#define ID3D11Debug_GetSwapChain(This,ppSwapChain)	\
    ( (This)->lpVtbl -> GetSwapChain(This,ppSwapChain) )

#define ID3D11Debug_ValidateContext(This,pContext)	\
    ( (This)->lpVtbl -> ValidateContext(This,pContext) )

#define ID3D11Debug_ReportLiveDeviceObjects(This,Flags)	\
    ( (This)->lpVtbl -> ReportLiveDeviceObjects(This,Flags) )

#define ID3D11Debug_ValidateContextForDispatch(This,pContext)	\
    ( (This)->lpVtbl -> ValidateContextForDispatch(This,pContext) )

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ID3D11Debug_INTERFACE_DEFINED__ */

DEFINE_GUID(IID_ID3D11Debug,0x79cf2233,0x7536,0x4948,0x9d,0x36,0x1e,0x46,0x92,0xdc,0x57,0x60);

#endif

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
DEFINE_GUID(DXVA_Intel_H264_NoFGT_ClearVideo,        0x604F8E68, 0x4951, 0x4c54,0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_VLD_WITHFMOASO_NOFGT,  0xd5f04ff9, 0x3418,0x45d8,0x95,0x61,0x32,0xa7,0x6a,0xae,0x2d,0xdd);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_VLD_STEREO_PROGRESSIVE_NOFGT, 0xd79be8da, 0x0cf1,0x4c81,0xb8,0x2a,0x69,0xa4,0xe2,0x36,0xf4,0x3d);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_VLD_STEREO_NOFGT,             0xf9aaccbb, 0xc2b6,0x4cfc,0x87,0x79,0x57,0x07,0xb1,0x76,0x05,0x52);
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_VLD_MULTIVIEW_NOFGT,          0x705b9d82, 0x76cf,0x49d6,0xb7,0xe6,0xac,0x88,0x72,0xdb,0x01,0x3c);
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

/* */
typedef struct {
    const char   *name;
    const GUID   *guid;
    int          codec;
    const int    *p_profiles; // NULL or ends with 0
} d3d11va_mode_t;

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
static const d3d11va_mode_t d3d11va_modes[] = {
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

static const d3d11va_mode_t *Dxva2FindMode(const GUID *guid)
{
    for (unsigned i = 0; d3d11va_modes[i].name; i++) {
        if (IsEqualGUID(d3d11va_modes[i].guid, guid))
            return &d3d11va_modes[i];
    }
    return NULL;
}

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

/* */
typedef struct {
    ID3D11VideoDecoderOutputView *d3d;
    int                refcount;
    unsigned int       order;
    vlc_mutex_t        *p_lock;
} vlc_va_surface_t;

#define VA_D3D11_MAX_SURFACE_COUNT (64)
struct vlc_va_sys_t
{
    int          codec_id;
    int          width;
    int          height;

    vlc_mutex_t     surface_lock;

    /* DLL */
    HINSTANCE             hd3d11_dll;
#ifndef NDEBUG
    HINSTANCE             dxgidebug_dll;
#endif

    /* Direct3D */
    ID3D11Device          *d3ddev;
    ID3D11DeviceContext   *d3dctx;

    /* Device manager */
    UINT                     token;

    /* Video service */
    ID3D11VideoDevice            *d3dvidev;
    ID3D11VideoContext           *d3dvidctx;
    GUID                         input;
    DXGI_FORMAT                  render;

    /* Video decoder */
    D3D11_VIDEO_DECODER_CONFIG    cfg;
    ID3D11VideoDecoder            *decoder;

    /* avcodec internals */
    struct AVD3D11VAContext       hw;

    /* Extraction */
    ID3D11Resource        *staging;
    copy_cache_t          *p_copy_cache;

    /* */
    int          surface_count;
    int          surface_order;
    int          surface_width;
    int          surface_height;
    vlc_fourcc_t surface_chroma;

    int          thread_count;

    vlc_va_surface_t surface[VA_D3D11_MAX_SURFACE_COUNT];
    ID3D11VideoDecoderOutputView* hw_surface[VA_D3D11_MAX_SURFACE_COUNT];
};

#if D3D11_DR /* for now we export to NV12 */
struct picture_sys_t
{
    ID3D11VideoDecoderOutputView *surface;
};
#endif

/* */
static int D3dCreateDevice(vlc_va_t *);
static void D3dDestroyDevice(vlc_va_sys_t *);
static char *DxDescribe(vlc_va_sys_t *);

#if 0
static int D3dCreateDeviceManager(vlc_va_t *);
static void D3dDestroyDeviceManager(vlc_va_sys_t *);
#endif

static int DxCreateVideoService(vlc_va_t *);
static void DxDestroyVideoService(vlc_va_sys_t *);
static int DxFindVideoServiceConversion(vlc_va_t *, GUID *input, DXGI_FORMAT *output, const es_format_t *fmt);

static int DxCreateVideoDecoder(vlc_va_t *,
                                int codec_id, const video_format_t *, bool);
static void DxDestroyVideoDecoder(vlc_va_sys_t *);
static int DxResetVideoDecoder(vlc_va_t *);

static bool profile_supported(const d3d11va_mode_t *mode, const es_format_t *fmt);

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
    sys->hw.video_context = sys->d3dvidctx;
    sys->hw.cfg = &sys->cfg;
    sys->hw.surface_count = sys->surface_count;
    sys->hw.surface = sys->hw_surface;

    /* */
ok:
    avctx->hwaccel_context = &sys->hw;
    *chroma = VLC_CODEC_NV12; /* TODO use an opaque format for direct rendering */

    return VLC_SUCCESS;
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

/* FIXME it is nearly common with VAAPI */
static int Get(vlc_va_t *va, picture_t *pic, uint8_t **data)
{
    vlc_va_sys_t *sys = va->sys;

#if TODO
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

    vlc_mutex_lock( &sys->surface_lock );

    /* Grab an unused surface, in case none are, try the oldest
     * XXX using the oldest is a workaround in case a problem happens with libavcodec */
    int i, old;
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
#if 0
    D3dDestroyDeviceManager(sys);
#endif
    D3dDestroyDevice(sys);

    if (sys->p_copy_cache!=NULL) {
        CopyCleanCache(sys->p_copy_cache);
        free( sys->p_copy_cache );
    }

    if (sys->hd3d11_dll)
        FreeLibrary(sys->hd3d11_dll);
#ifndef NDEBUG
    if (sys->dxgidebug_dll)
        FreeLibrary(sys->dxgidebug_dll);
#endif
#if DEBUG_LEAK
    sys->hd3d11_dll = NULL;
    sys->dxgidebug_dll = NULL;
#endif
    vlc_mutex_destroy( &sys->surface_lock );

    free((char *)va->description);
    free(sys);
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

static int Open(vlc_va_t *va, AVCodecContext *ctx, enum PixelFormat pix_fmt,
                const es_format_t *fmt, picture_sys_t *p_sys)
{
    int err = VLC_EGENERIC;

    if (pix_fmt != AV_PIX_FMT_D3D11VA_VLD)
        return VLC_EGENERIC;

    VLC_UNUSED(p_sys);

    vlc_va_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->p_copy_cache = calloc(1, sizeof(*sys->p_copy_cache));
    if (!sys->p_copy_cache) {
         err = VLC_ENOMEM;
         goto error;
    }
    CopyInitCache( sys->p_copy_cache, fmt->video.i_width );

    va->sys = sys;
    sys->codec_id = ctx->codec_id;

    vlc_mutex_init( &sys->surface_lock );

    /* Load dll*/
    sys->hd3d11_dll = LoadLibrary(TEXT("D3D11.DLL"));
    if (!sys->hd3d11_dll) {
        msg_Warn(va, "cannot load d3d11.dll");
        goto error;
    }
    msg_Dbg(va, "DLLs loaded");

#ifndef NDEBUG
    sys->dxgidebug_dll = LoadLibrary(TEXT("DXGIDEBUG.DLL"));
#endif

    sys->d3ddev = NULL;
#if D3D11_DR
    if ( p_sys != NULL )
        sys->d3ddev = GetOutputViewDevice( p_sys->surface );
#endif

    if (sys->d3ddev) {
        msg_Dbg(va, "Reusing D3D11 device");
    } else {
        /* */
        if (D3dCreateDevice(va)) {
            msg_Err(va, "Failed to create Direct3D device");
            goto error;
        }
        msg_Dbg(va, "D3dCreateDevice succeed");
    }

#if 0
    if (D3dCreateDeviceManager(va)) {
        msg_Err(va, "D3dCreateDeviceManager failed");
        goto error;
    }
#endif

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
    return err;
}
/* */

/**
 * It creates a Direct3D device usable for decoding
 */
static int D3dCreateDevice(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    HRESULT hr;

    /* */
    PFN_D3D11_CREATE_DEVICE pf_CreateDevice;
    pf_CreateDevice = (void *)GetProcAddress(sys->hd3d11_dll, "D3D11CreateDevice");
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
    sys->d3ddev = d3ddev;
    sys->d3dctx = d3dctx;
#if DEBUG_LEAK
    msg_Err(va, "Created ID3D11Device 0x%p / ID3D11DeviceContext 0x%p", d3ddev, d3dctx);
#endif

    ID3D11VideoContext *d3dvidctx = NULL;
    hr = ID3D11DeviceContext_QueryInterface(d3dctx, &IID_ID3D11VideoContext, (void **)&d3dvidctx);
    if (FAILED(hr)) {
       msg_Err(va, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    sys->d3dvidctx = d3dvidctx;
#if DEBUG_LEAK
    msg_Err(va, "Got ID3D11VideoContext 0x%p", d3dvidctx);
#endif

#ifndef NDEBUG
    HRESULT (WINAPI  * pf_DXGIGetDebugInterface)(const GUID *riid, void **ppDebug);
    if (sys->dxgidebug_dll) {
        pf_DXGIGetDebugInterface = (void *)GetProcAddress(sys->dxgidebug_dll, "DXGIGetDebugInterface");
        if (pf_DXGIGetDebugInterface) {
            IDXGIDebug *pDXGIDebug = NULL;
            hr = pf_DXGIGetDebugInterface(&IID_IDXGIDebug, (void**)&pDXGIDebug);
            if (SUCCEEDED(hr) && pDXGIDebug) {
                hr = IDXGIDebug_ReportLiveObjects(pDXGIDebug, DXGI_DEBUG_DX, DXGI_DEBUG_RLO_DETAIL);
            }
        }
    }

    ID3D11Debug *pD3D11Debug;
    hr = ID3D11Device_QueryInterface(d3ddev, &IID_ID3D11Debug, (void **) &pD3D11Debug);
    if (SUCCEEDED(hr)) {
        hr = ID3D11Debug_ReportLiveDeviceObjects(pD3D11Debug, D3D11_RLDO_DETAIL);
        ID3D11Debug_Release(pD3D11Debug);
    }
#endif

    return VLC_SUCCESS;
}

/**
 * It releases a Direct3D device and its resources.
 */
static void D3dDestroyDevice(vlc_va_sys_t *va)
{
    if (va->d3dvidctx)
        ID3D11VideoContext_Release(va->d3dvidctx);
    if (va->d3dctx)
        ID3D11DeviceContext_Release(va->d3dctx);
    if (va->d3ddev)
        ID3D11Device_Release(va->d3ddev);
#if DEBUG_LEAK
    va->d3dvidctx = NULL;
    va->d3dctx = NULL;
    va->d3ddev = NULL;
#endif
}
/**
 * It describes our Direct3D object
 */
static char *DxDescribe(vlc_va_sys_t *p_sys)
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
    HRESULT hr = ID3D11Device_QueryInterface(p_sys->d3ddev, &IID_IDXGIDevice, (void **)&pDXGIDevice);
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

#if 0
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
#endif

/**
 * It creates a DirectX video service
 */
static int DxCreateVideoService(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    ID3D11VideoDevice *d3dvidev = NULL;
    HRESULT hr = ID3D11Device_QueryInterface(sys->d3ddev, &IID_ID3D11VideoDevice, (void **)&d3dvidev);
    if (FAILED(hr)) {
       msg_Err(va, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    sys->d3dvidev = d3dvidev;
#if DEBUG_LEAK
    msg_Err(va, "Got ID3D11VideoDevice 0x%p", d3dvidev);
#endif

    return VLC_SUCCESS;
}

/**
 * It destroys a DirectX video service
 */
static void DxDestroyVideoService(vlc_va_sys_t *va)
{
    if (va->d3dvidev)
        ID3D11VideoDevice_Release(va->d3dvidev);
#if DEBUG_LEAK
    va->d3dvidev = NULL;
#endif
}

static bool profile_supported(const d3d11va_mode_t *mode, const es_format_t *fmt)
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
    HRESULT hr;

    /* Retreive supported modes from the decoder service */
    UINT input_count = ID3D11VideoDevice_GetVideoDecoderProfileCount(sys->d3dvidev);
    GUID input_list[input_count];
    memset(input_list, 0, sizeof(input_list));
    for (UINT i = 0; i < input_count; i++) {
        hr = ID3D11VideoDevice_GetVideoDecoderProfile(sys->d3dvidev, i, &input_list[i]);
        if (FAILED(hr))
        {
            msg_Err(va, "GetVideoDecoderProfile %d failed. (hr=0x%lX)", i, hr);
            continue;
        }
    }
    for (unsigned i = 0; i < input_count; i++) {
        const GUID *g = &input_list[i];
        const d3d11va_mode_t *mode = Dxva2FindMode(g);
        if (mode) {
            msg_Dbg(va, "- '%s' is supported by hardware", mode->name);
        } else {
            msg_Warn(va, "- Unknown GUID = " GUID_FMT, GUID_PRINT( *g ) );
        }
    }

    /* Try all supported mode by our priority */
    for (unsigned i = 0; d3d11va_modes[i].name; i++) {
        const d3d11va_mode_t *mode = &d3d11va_modes[i];
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
                msg_Warn( va, "Unsupported profile for D3D11 HWAccel: %d", fmt->i_profile );
        }
        if (!is_supported)
            continue;

        /* */
        msg_Dbg(va, "Trying to use '%s' as input", mode->name);
        for (unsigned j = 0; d3d_formats[j].name; j++) {
            const d3d_format_t *format = &d3d_formats[j];

            BOOL is_supported = false;
            hr = ID3D11VideoDevice_CheckVideoDecoderFormat(sys->d3dvidev, mode->guid, format->format, &is_supported);
            if (FAILED(hr) || !is_supported)
                continue;
            msg_Dbg(va, "%s is supported for output", format->name);
        }

        /* */
        for (unsigned j = 0; d3d_formats[j].name; j++) {
            const d3d_format_t *format = &d3d_formats[j];

            /* */
            BOOL is_supported = false;
            hr = ID3D11VideoDevice_CheckVideoDecoderFormat(sys->d3dvidev, mode->guid, format->format, &is_supported);
            if (FAILED(hr) || !is_supported)
                continue;

            /* We have our solution */
            msg_Dbg(va, "Using '%s' to decode to '%s'", mode->name, format->name);
            *input  = *mode->guid;
            *output = format->format;
            return VLC_SUCCESS;
        }
    }
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
    HRESULT hr;

    ID3D10Multithread *pMultithread;
    hr = ID3D11Device_QueryInterface(sys->d3ddev, &IID_ID3D10Multithread, (void **)&pMultithread);
    if (SUCCEEDED(hr)) {
        ID3D10Multithread_SetMultithreadProtected(pMultithread, b_threading && sys->thread_count > 1);
        ID3D10Multithread_Release(pMultithread);
#if DEBUG_LEAK
    msg_Err(va, "Got ID3D10Multithread 0x%p", pMultithread);
#endif
    }

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

    if (surface_count > VA_D3D11_MAX_SURFACE_COUNT)
        return VLC_EGENERIC;

    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width = sys->surface_width;
    texDesc.Height = sys->surface_height;
    texDesc.MipLevels = 1;
    texDesc.Format = sys->render;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0; // D3D11_RESOURCE_MISC_SHARED
    texDesc.ArraySize = 1;
    texDesc.Usage = D3D11_USAGE_STAGING;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    texDesc.BindFlags = 0;

    sys->staging = NULL;
    hr = ID3D11Device_CreateTexture2D( sys->d3ddev, &texDesc, NULL,
                                       (ID3D11Texture2D**) &sys->staging);
    if (FAILED(hr)) {
        msg_Err(va, "Failed to create a staging texture to extract surface pixels (hr=0x%0lx)", hr );
        return VLC_EGENERIC;
    }

    texDesc.ArraySize = surface_count;
    texDesc.Usage = D3D11_USAGE_DEFAULT; //D3D11_USAGE_DYNAMIC; //D3D11_USAGE_STAGING; // D3D11_USAGE_DEFAULT
    texDesc.BindFlags = D3D11_BIND_DECODER;// | D3D11_BIND_UNORDERED_ACCESS;
    texDesc.CPUAccessFlags = 0; //D3D11_CPU_ACCESS_READ;

    ID3D11Texture2D *p_texture;
    hr = ID3D11Device_CreateTexture2D( sys->d3ddev, &texDesc, NULL, &p_texture );
    if (FAILED(hr)) {
        msg_Err(va, "CreateTexture2D %d failed. (hr=0x%0lx)", sys->surface_count, hr);
        return VLC_EGENERIC;
    }

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    ZeroMemory(&viewDesc, sizeof(viewDesc));
    viewDesc.DecodeProfile = sys->input;
    viewDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;

    for (sys->surface_count = 0; sys->surface_count < surface_count; sys->surface_count++) {
        vlc_va_surface_t *surface = &sys->surface[sys->surface_count];

        viewDesc.Texture2D.ArraySlice = sys->surface_count;

        ID3D11Resource *p_resource = (ID3D11Resource*) p_texture;
        hr = ID3D11VideoDevice_CreateVideoDecoderOutputView( sys->d3dvidev, p_resource,
                                                             &viewDesc, &surface->d3d );
        if (FAILED(hr)) {
            msg_Err(va, "CreateVideoDecoderOutputView %d failed. (hr=0x%0lx)", sys->surface_count, hr);
            ID3D11Texture2D_Release(p_texture);
            return VLC_EGENERIC;
        }
        surface->refcount = 0;
        surface->order = 0;
        surface->p_lock = &sys->surface_lock;
        sys->hw_surface[sys->surface_count] = surface->d3d;
    }
    msg_Dbg(va, "ID3D11VideoDecoderOutputView succeed with %d surfaces (%dx%d)",
            sys->surface_count, fmt->i_width, fmt->i_height);

    D3D11_VIDEO_DECODER_DESC decoderDesc;
    ZeroMemory(&decoderDesc, sizeof(decoderDesc));
    decoderDesc.Guid = sys->input;
    decoderDesc.SampleWidth = sys->surface_width;
    decoderDesc.SampleHeight = sys->surface_height;
    decoderDesc.OutputFormat = sys->render;

    UINT cfg_count;
    hr = ID3D11VideoDevice_GetVideoDecoderConfigCount( sys->d3dvidev, &decoderDesc, &cfg_count );
    if (FAILED(hr)) {
        msg_Err(va, "GetVideoDecoderConfigCount failed. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    /* List all configurations available for the decoder */
    D3D11_VIDEO_DECODER_CONFIG cfg_list[cfg_count];
    for (unsigned i = 0; i < cfg_count; i++) {
        hr = ID3D11VideoDevice_GetVideoDecoderConfig( sys->d3dvidev, &decoderDesc, i, &cfg_list[i] );
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
    hr = ID3D11VideoDevice_CreateVideoDecoder( sys->d3dvidev, &decoderDesc, &sys->cfg, &sys->decoder );
    if (FAILED(hr)) {
        msg_Err(va, "ID3D11VideoDevice_CreateVideoDecoder failed. (hr=0x%lX)", hr);
        sys->decoder = NULL;
        return VLC_EGENERIC;
    }

    if (IsEqualGUID(&sys->input, &DXVA_Intel_H264_NoFGT_ClearVideo))
        sys->hw.workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;

    msg_Dbg(va, "DxCreateVideoDecoder succeed");
    return VLC_SUCCESS;
}

static void DxDestroyVideoDecoder(vlc_va_sys_t *va)
{
    for (int i = 0; i < va->surface_count; i++) {
        ID3D11Resource *p_texture;
        ID3D11VideoDecoderOutputView_GetResource( va->surface[i].d3d, &p_texture );
        ID3D11Resource_Release(p_texture);
        ID3D11VideoDecoderOutputView_Release(va->surface[i].d3d);
#if DEBUG_LEAK
        assert(va->surface[i].d3d == va->hw_surface[i]);
        va->surface[i].d3d = va->hw_surface[i] = NULL;
#endif
    }
    va->surface_count = 0;

    if (va->staging)
        ID3D11Resource_Release(va->staging);
    if (va->decoder)
        ID3D11VideoDecoder_Release(va->decoder);
    va->decoder = NULL;
#if DEBUG_LEAK
    va->staging = NULL;
#endif
}

static int DxResetVideoDecoder(vlc_va_t *va)
{
    msg_Err(va, "DxResetVideoDecoder unimplemented");
    return VLC_EGENERIC;
}


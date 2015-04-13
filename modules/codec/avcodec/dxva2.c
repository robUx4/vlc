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
#include <vlc_cpu.h>
#include <vlc_plugin.h>

#include <libavcodec/avcodec.h>
#    define DXVA2API_USE_BITFIELDS
#    define COBJMACROS
#    include <libavcodec/dxva2.h>

#include "avcodec.h"
#include "va.h"
#include "../../demux/asf/libasf_guid.h"
#include "../../video_output/msw/direct3d9.h"
#include "dxva2.h"

#define GET_DECODER_SURFACE 0

static int Open(vlc_va_t *, AVCodecContext *, const es_format_t *);
static void Close(vlc_va_t *, AVCodecContext *);

vlc_module_begin()
    set_description(N_("DirectX Video Acceleration (DXVA) 2.0"))
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
#include <d3d9.h>
#include <dxva2api.h>

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

/* Codec capabilities GUID, sorted by codec */
MS_GUID    (DXVA2_ModeMPEG2_MoComp,                 0xe6a9f44b, 0x61b0, 0x4563, 0x9e, 0xa4, 0x63, 0xd2, 0xa3, 0xc6, 0xfe, 0x66);
MS_GUID    (DXVA2_ModeMPEG2_IDCT,                   0xbf22ad00, 0x03ea, 0x4690, 0x80, 0x77, 0x47, 0x33, 0x46, 0x20, 0x9b, 0x7e);
MS_GUID    (DXVA2_ModeMPEG2_VLD,                    0xee27417f, 0x5e28, 0x4e65, 0xbe, 0xea, 0x1d, 0x26, 0xb5, 0x08, 0xad, 0xc9);
DEFINE_GUID(DXVA_ModeMPEG1_A,                       0x1b81be09, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA_ModeMPEG2_A,                       0x1b81be0A, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA_ModeMPEG2_B,                       0x1b81be0B, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA_ModeMPEG2_C,                       0x1b81be0C, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA_ModeMPEG2_D,                       0x1b81be0D, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA2_ModeMPEG2and1_VLD,                0x86695f12, 0x340e, 0x4f04, 0x9f, 0xd3, 0x92, 0x53, 0xdd, 0x32, 0x74, 0x60);
DEFINE_GUID(DXVA2_ModeMPEG1_VLD,                    0x6f3ec719, 0x3735, 0x42cc, 0x80, 0x63, 0x65, 0xcc, 0x3c, 0xb3, 0x66, 0x16);

MS_GUID    (DXVA2_ModeH264_A,                       0x1b81be64, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
MS_GUID    (DXVA2_ModeH264_B,                       0x1b81be65, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
MS_GUID    (DXVA2_ModeH264_C,                       0x1b81be66, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
MS_GUID    (DXVA2_ModeH264_D,                       0x1b81be67, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
MS_GUID    (DXVA2_ModeH264_E,                       0x1b81be68, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
MS_GUID    (DXVA2_ModeH264_F,                       0x1b81be69, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA2_ModeH264_G,                       0x1b81be6a, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA_ModeH264_VLD_Multiview,            0x9901CCD3, 0xca12, 0x4b7e, 0x86, 0x7a, 0xe2, 0x22, 0x3d, 0x92, 0x55, 0xc3); // MVC
DEFINE_GUID(DXVA_ModeH264_VLD_WithFMOASO_NoFGT,     0xd5f04ff9, 0x3418, 0x45d8, 0x95, 0x61, 0x32, 0xa7, 0x6a, 0xae, 0x2d, 0xdd);
DEFINE_GUID(DXVADDI_Intel_ModeH264_A,               0x604F8E64, 0x4951, 0x4c54, 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6);
DEFINE_GUID(DXVADDI_Intel_ModeH264_C,               0x604F8E66, 0x4951, 0x4c54, 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6);
DEFINE_GUID(DXVADDI_Intel_ModeH264_E,               0x604F8E68, 0x4951, 0x4c54, 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6); // DXVA_Intel_H264_NoFGT_ClearVideo
DEFINE_GUID(DXVA_ModeH264_VLD_NoFGT_Flash,          0x4245F676, 0x2BBC, 0x4166, 0xa0, 0xBB, 0x54, 0xE7, 0xB8, 0x49, 0xC3, 0x80);

MS_GUID    (DXVA2_ModeWMV8_A,                       0x1b81be80, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
MS_GUID    (DXVA2_ModeWMV8_B,                       0x1b81be81, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);

MS_GUID    (DXVA2_ModeWMV9_A,                       0x1b81be90, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
MS_GUID    (DXVA2_ModeWMV9_B,                       0x1b81be91, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
MS_GUID    (DXVA2_ModeWMV9_C,                       0x1b81be94, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);

MS_GUID    (DXVA2_ModeVC1_A,                        0x1b81beA0, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
MS_GUID    (DXVA2_ModeVC1_B,                        0x1b81beA1, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
MS_GUID    (DXVA2_ModeVC1_C,                        0x1b81beA2, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
MS_GUID    (DXVA2_ModeVC1_D,                        0x1b81beA3, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA2_ModeVC1_D2010,                    0x1b81beA4, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5); // August 2010 update
DEFINE_GUID(DXVA_Intel_VC1_ClearVideo,              0xBCC5DB6D, 0xA2B6, 0x4AF0, 0xAC, 0xE4, 0xAD, 0xB1, 0xF7, 0x87, 0xBC, 0x89);
DEFINE_GUID(DXVA_Intel_VC1_ClearVideo_2,            0xE07EC519, 0xE651, 0x4CD6, 0xAC, 0x84, 0x13, 0x70, 0xCC, 0xEE, 0xC8, 0x51);

DEFINE_GUID(DXVA_nVidia_MPEG4_ASP,                  0x9947EC6F, 0x689B, 0x11DC, 0xA3, 0x20, 0x00, 0x19, 0xDB, 0xBC, 0x41, 0x84);
DEFINE_GUID(DXVA_ModeMPEG4pt2_VLD_Simple,           0xefd64d74, 0xc9e8, 0x41d7, 0xa5, 0xe9, 0xe9, 0xb0, 0xe3, 0x9f, 0xa3, 0x19);
DEFINE_GUID(DXVA_ModeMPEG4pt2_VLD_AdvSimple_NoGMC,  0xed418a9f, 0x010d, 0x4eda, 0x9a, 0xe3, 0x9a, 0x65, 0x35, 0x8d, 0x8d, 0x2e);
DEFINE_GUID(DXVA_ModeMPEG4pt2_VLD_AdvSimple_GMC,    0xab998b5b, 0x4258, 0x44a9, 0x9f, 0xeb, 0x94, 0xe5, 0x97, 0xa6, 0xba, 0xae);
DEFINE_GUID(DXVA_ModeMPEG4pt2_VLD_AdvSimple_Avivo,  0x7C74ADC6, 0xe2ba, 0x4ade, 0x86, 0xde, 0x30, 0xbe, 0xab, 0xb4, 0x0c, 0xc1);

DEFINE_GUID(DXVA_ModeHEVC_VLD_Main,                 0x5b11d51b, 0x2f4c, 0x4452,0xbc,0xc3,0x09,0xf2,0xa1,0x16,0x0c,0xc0);
DEFINE_GUID(DXVA_ModeHEVC_VLD_Main10,               0x107af0e0, 0xef1a, 0x4d19,0xab,0xa8,0x67,0xa1,0x63,0x07,0x3d,0x13);

DEFINE_GUID(DXVA_ModeH264_VLD_Stereo_Progressive_NoFGT,     0xd79be8da, 0x0cf1, 0x4c81,0xb8,0x2a,0x69,0xa4,0xe2,0x36,0xf4,0x3d);
DEFINE_GUID(DXVA_ModeH264_VLD_Stereo_NoFGT,                 0xf9aaccbb, 0xc2b6, 0x4cfc,0x87,0x79,0x57,0x07,0xb1,0x76,0x05,0x52);
DEFINE_GUID(DXVA_ModeH264_VLD_Multiview_NoFGT,              0x705b9d82, 0x76cf, 0x49d6,0xb7,0xe6,0xac,0x88,0x72,0xdb,0x01,0x3c);

DEFINE_GUID(DXVA_ModeH264_VLD_SVC_Scalable_Baseline,                    0xc30700c4, 0xe384, 0x43e0, 0xb9, 0x82, 0x2d, 0x89, 0xee, 0x7f, 0x77, 0xc4);
DEFINE_GUID(DXVA_ModeH264_VLD_SVC_Restricted_Scalable_Baseline,         0x9b8175d4, 0xd670, 0x4cf2, 0xa9, 0xf0, 0xfa, 0x56, 0xdf, 0x71, 0xa1, 0xae);
DEFINE_GUID(DXVA_ModeH264_VLD_SVC_Scalable_High,                        0x728012c9, 0x66a8, 0x422f, 0x97, 0xe9, 0xb5, 0xe3, 0x9b, 0x51, 0xc0, 0x53);
DEFINE_GUID(DXVA_ModeH264_VLD_SVC_Restricted_Scalable_High_Progressive, 0x8efa5926, 0xbd9e, 0x4b04, 0x8b, 0x72, 0x8f, 0x97, 0x7d, 0xc4, 0x4c, 0x36);

DEFINE_GUID(DXVA_ModeH261_A,                        0x1b81be01, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA_ModeH261_B,                        0x1b81be02, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);

DEFINE_GUID(DXVA_ModeH263_A,                        0x1b81be03, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA_ModeH263_B,                        0x1b81be04, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA_ModeH263_C,                        0x1b81be05, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA_ModeH263_D,                        0x1b81be06, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA_ModeH263_E,                        0x1b81be07, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA_ModeH263_F,                        0x1b81be08, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);


/* */
typedef struct {
    const char   *name;
    const GUID   *guid;
    int          codec_id;
    int          i_profile; // 0 means no profile constraint
} dxva2_mode_t;
/* XXX Prefered modes must come first */
static const dxva2_mode_t dxva2_modes[] = {
    /* MPEG-1/2 */
    { "MPEG-1 decoder, restricted profile A",                                         &DXVA_ModeMPEG1_A,                      0, 0 },
    { "MPEG-2 decoder, restricted profile A",                                         &DXVA_ModeMPEG2_A,                      0, 0 },
    { "MPEG-2 decoder, restricted profile B",                                         &DXVA_ModeMPEG2_B,                      0, 0 },
    { "MPEG-2 decoder, restricted profile C",                                         &DXVA_ModeMPEG2_C,                      0, 0 },
    { "MPEG-2 decoder, restricted profile D",                                         &DXVA_ModeMPEG2_D,                      0, 0 },
    { "MPEG-2 variable-length decoder",                                               &DXVA2_ModeMPEG2_VLD,                   AV_CODEC_ID_MPEG2VIDEO, FF_PROFILE_MPEG2_SIMPLE },
    { "MPEG-2 & MPEG-1 variable-length decoder",                                      &DXVA2_ModeMPEG2and1_VLD,               AV_CODEC_ID_MPEG2VIDEO, FF_PROFILE_MPEG2_MAIN },
    { "MPEG-2 motion compensation",                                                   &DXVA2_ModeMPEG2_MoComp,                0, 0 },
    { "MPEG-2 inverse discrete cosine transform",                                     &DXVA2_ModeMPEG2_IDCT,                  0, 0 },

    /* MPEG-1 http://download.microsoft.com/download/B/1/7/B172A3C8-56F2-4210-80F1-A97BEA9182ED/DXVA_MPEG1_VLD.pdf */
    { "MPEG-1 variable-length decoder, no D pictures",                                &DXVA2_ModeMPEG1_VLD,                   0, 0 },

    /* H.264 http://www.microsoft.com/downloads/details.aspx?displaylang=en&FamilyID=3d1c290b-310b-4ea2-bf76-714063a6d7a6 */
    { "H.264 variable-length decoder, film grain technology",                         &DXVA2_ModeH264_F,                      AV_CODEC_ID_H264, FF_PROFILE_H264_HIGH },
    { "H.264 variable-length decoder, no film grain technology (Intel ClearVideo)",   &DXVADDI_Intel_ModeH264_E,              AV_CODEC_ID_H264, FF_PROFILE_H264_HIGH },
    { "H.264 variable-length decoder, no film grain technology",                      &DXVA2_ModeH264_E,                      AV_CODEC_ID_H264, FF_PROFILE_H264_HIGH },
    { "H.264 variable-length decoder, no film grain technology, FMO/ASO",             &DXVA_ModeH264_VLD_WithFMOASO_NoFGT,    AV_CODEC_ID_H264, FF_PROFILE_H264_HIGH },
    { "H.264 variable-length decoder, no film grain technology, Flash",               &DXVA_ModeH264_VLD_NoFGT_Flash,         AV_CODEC_ID_H264, FF_PROFILE_H264_HIGH },
    { "H.264 variable-length decoder, undocumented",                                  &DXVA2_ModeH264_G,                      0, 0 },

    { "H.264 inverse discrete cosine transform, film grain technology",               &DXVA2_ModeH264_D,                      0, 0 },
    { "H.264 inverse discrete cosine transform, no film grain technology",            &DXVA2_ModeH264_C,                      0, 0 },
    { "H.264 inverse discrete cosine transform, no film grain technology (Intel)",    &DXVADDI_Intel_ModeH264_C,              0, 0 },

    { "H.264 motion compensation, film grain technology",                             &DXVA2_ModeH264_B,                      0, 0 },
    { "H.264 motion compensation, no film grain technology",                          &DXVA2_ModeH264_A,                      0, 0 },
    { "H.264 motion compensation, no film grain technology (Intel)",                  &DXVADDI_Intel_ModeH264_A,              0, 0 },

    /* http://download.microsoft.com/download/2/D/0/2D02E72E-7890-430F-BA91-4A363F72F8C8/DXVA_H264_MVC.pdf */
    { "H.264 stereo high profile, mbs flag set",                                      &DXVA_ModeH264_VLD_Stereo_Progressive_NoFGT, 0, 0 },
    { "H.264 stereo high profile",                                                    &DXVA_ModeH264_VLD_Stereo_NoFGT,             0, 0 },
    { "H.264 multiview high profile",                                                 &DXVA_ModeH264_VLD_Multiview_NoFGT,          0, 0 },

    /* SVC http://download.microsoft.com/download/C/8/A/C8AD9F1B-57D1-4C10-85A0-09E3EAC50322/DXVA_SVC_2012_06.pdf */
    { "H.264 scalable video coding, Scalable Baseline Profile",                       &DXVA_ModeH264_VLD_SVC_Scalable_Baseline,            0, 0 },
    { "H.264 scalable video coding, Scalable Constrained Baseline Profile",           &DXVA_ModeH264_VLD_SVC_Restricted_Scalable_Baseline, 0, 0 },
    { "H.264 scalable video coding, Scalable High Profile",                           &DXVA_ModeH264_VLD_SVC_Scalable_High,                0, 0 },
    { "H.264 scalable video coding, Scalable Constrained High Profile",               &DXVA_ModeH264_VLD_SVC_Restricted_Scalable_High_Progressive, 0, 0 },

    /* WMV */
    { "Windows Media Video 8 motion compensation",                                    &DXVA2_ModeWMV8_B,                      0, 0 },
    { "Windows Media Video 8 post processing",                                        &DXVA2_ModeWMV8_A,                      0, 0 },

    { "Windows Media Video 9 IDCT",                                                   &DXVA2_ModeWMV9_C,                      0, 0 },
    { "Windows Media Video 9 motion compensation",                                    &DXVA2_ModeWMV9_B,                      0, 0 },
    { "Windows Media Video 9 post processing",                                        &DXVA2_ModeWMV9_A,                      0, 0 },

    /* VC-1 */
    { "VC-1 variable-length decoder",                                                 &DXVA2_ModeVC1_D,                       AV_CODEC_ID_VC1, 0 },
    { "VC-1 variable-length decoder",                                                 &DXVA2_ModeVC1_D,                       AV_CODEC_ID_WMV3, 0 },
    { "VC-1 variable-length decoder",                                                 &DXVA2_ModeVC1_D2010,                   AV_CODEC_ID_VC1, 0 },
    { "VC-1 variable-length decoder",                                                 &DXVA2_ModeVC1_D2010,                   AV_CODEC_ID_WMV3, 0 },
    { "VC-1 variable-length decoder 2 (Intel)",                                       &DXVA_Intel_VC1_ClearVideo_2,           0, 0 },
    { "VC-1 variable-length decoder (Intel)",                                         &DXVA_Intel_VC1_ClearVideo,             0, 0 },

    { "VC-1 inverse discrete cosine transform",                                       &DXVA2_ModeVC1_C,                       0, 0 },
    { "VC-1 motion compensation",                                                     &DXVA2_ModeVC1_B,                       0, 0 },
    { "VC-1 post processing",                                                         &DXVA2_ModeVC1_A,                       0, 0 },

    /* Xvid/Divx: TODO */
    { "MPEG-4 Part 2 nVidia bitstream decoder",                                       &DXVA_nVidia_MPEG4_ASP,                 0, 0 },
    { "MPEG-4 Part 2 variable-length decoder, Simple Profile",                        &DXVA_ModeMPEG4pt2_VLD_Simple,          0, 0 },
    { "MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, no GMC",       &DXVA_ModeMPEG4pt2_VLD_AdvSimple_NoGMC, 0, 0 },
    { "MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, GMC",          &DXVA_ModeMPEG4pt2_VLD_AdvSimple_GMC,   0, 0 },
    { "MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, Avivo",        &DXVA_ModeMPEG4pt2_VLD_AdvSimple_Avivo, 0, 0 },

    /* HEVC */
    { "HEVC Main profile",                                                            &DXVA_ModeHEVC_VLD_Main,                AV_CODEC_ID_HEVC, FF_PROFILE_HEVC_MAIN },
    { "HEVC Main 10 profile",                                                         &DXVA_ModeHEVC_VLD_Main10,              AV_CODEC_ID_HEVC, FF_PROFILE_HEVC_MAIN_10 },

    /* H.261 */
    { "H.261 decoder, restricted profile A",                                          &DXVA_ModeH261_A,                       0, 0 },
    { "H.261 decoder, restricted profile B",                                          &DXVA_ModeH261_B,                       0, 0 },

    /* H.261 */
    { "H.263 decoder, restricted profile A",                                          &DXVA_ModeH263_A,                       0, 0 },
    { "H.263 decoder, restricted profile B",                                          &DXVA_ModeH263_B,                       0, 0 },
    { "H.263 decoder, restricted profile C",                                          &DXVA_ModeH263_C,                       0, 0 },
    { "H.263 decoder, restricted profile D",                                          &DXVA_ModeH263_D,                       0, 0 },
    { "H.263 decoder, restricted profile E",                                          &DXVA_ModeH263_E,                       0, 0 },
    { "H.263 decoder, restricted profile F",                                          &DXVA_ModeH263_F,                       0, 0 },

    { NULL, NULL, 0, 0 }
};

static const dxva2_mode_t *Dxva2FindMode(const GUID *guid)
{
    for (unsigned i = 0; dxva2_modes[i].name; i++) {
        if (IsEqualGUID(dxva2_modes[i].guid, guid))
            return &dxva2_modes[i];
    }
    return NULL;
}


/* XXX Prefered format must come first */
static const d3d_format_t d3d_formats[] = {
    { "YV12",   MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_YV12, 0,0,0 },
    { "NV12",   MAKEFOURCC('N','V','1','2'),    VLC_CODEC_NV12, 0,0,0 },
    { "IMC3",   MAKEFOURCC('I','M','C','3'),    VLC_CODEC_YV12, 0,0,0 },

    { NULL, 0, 0, 0,0,0 }
};

static const d3d_format_t *D3dFindFormat(D3DFORMAT format)
{
    for (unsigned i = 0; d3d_formats[i].name; i++) {
        if (d3d_formats[i].format == format)
            return &d3d_formats[i];
    }
    return NULL;
}

/* */
static int D3dCreateDevice(vlc_va_t *);
static void D3dDestroyDevice(vlc_va_sys_t *);
static char *DxDescribe(vlc_va_sys_t *);

static int D3dCreateDeviceManager(vlc_va_t *);
static void D3dDestroyDeviceManager(vlc_va_sys_t *);

static int DxCreateVideoService(vlc_va_t *);
static void DxDestroyVideoService(vlc_va_sys_t *);
static int DxFindVideoServiceConversion(vlc_va_t *, GUID *input, const d3d_format_t **output);

static int DxCreateVideoDecoder(vlc_va_t *,
                                int codec_id, const video_format_t *, int);
static void DxDestroyVideoDecoder(vlc_va_sys_t *);
static int DxResetVideoDecoder(vlc_va_t *);

/* */
static int Setup(vlc_va_t *va, AVCodecContext *avctx, vlc_fourcc_t *chroma)
{
    vlc_va_sys_t *sys = va->sys;

    if (sys->width == avctx->coded_width && sys->height == avctx->coded_height
     && sys->decoder != NULL)
        goto ok;

#if DEBUG_SURFACE
    msg_Dbg( va, "Setup new decoder in 0x%p %dx%d was %dx%d 0x%p", va, avctx->coded_width, avctx->coded_height,
             sys->width, sys->height, sys->decoder );
#endif
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
    sys->output = sys->p_render->format;

    /* */
ok:
    avctx->hwaccel_context = &sys->hw;
    *chroma = VLC_CODEC_DXVA_D3D9_OPAQUE;

    return VLC_SUCCESS;
}

static int Extract(vlc_va_t *va, picture_t *picture, void *opaque,
                   uint8_t *data)
{
    picture_sys_t *p_picture_sys = opaque;

    //p_picture_sys = &va->sys->surface[4];

    vlc_va_sys_t *sys = va->sys;
    LPDIRECT3DSURFACE9 source = (LPDIRECT3DSURFACE9)(uintptr_t)data;

    /* */
    if ( p_picture_sys->p_lock )
        vlc_mutex_lock( p_picture_sys->p_lock );

#if GET_DECODER_SURFACE
    D3DLOCKED_RECT lock;
    if (FAILED(IDirect3DSurface9_LockRect(source, &lock, NULL, D3DLOCK_READONLY))) {
        msg_Err(va, "Failed to lock surface");
        if ( p_picture_sys->p_lock )
            vlc_mutex_unlock( p_picture_sys->p_lock );

        return VLC_EGENERIC;
    }
    p_picture_sys->b_lockrect = true;
#if DEBUG_SURFACE
    msg_Dbg(va, "d3d surface 0x%p at %d 0x%p locked", data, p_picture_sys->index, p_picture_sys );
#endif

#endif

    D3DSURFACE_DESC srcDesc;
    HRESULT hr = IDirect3DSurface9_GetDesc( source, &srcDesc);
    D3DSURFACE_DESC dstDesc;
    if ( sys->decoder_surface_idx >= sys->decoder_surface_num )
        sys->decoder_surface_idx = 0;
//sys->decoder_surface_idx = p_picture_sys->index;
    hr = IDirect3DSurface9_GetDesc( sys->decoder_pictures[sys->decoder_surface_idx].surface, &dstDesc);
    // TODO find an unused "decoder" surface/picture and copy the lock surface to it
    hr = IDirect3DDevice9_StretchRect( sys->d3ddev, source, NULL, sys->decoder_pictures[sys->decoder_surface_idx].surface, NULL, D3DTEXF_LINEAR);
    //hr = IDirect3DDevice9_UpdateSurface( sys->d3ddev, d3d, NULL, sys->decoder_surface[0], NULL );
    if (FAILED(hr)) {
        msg_Err(va, "Failed to copy the hw surface to the decoder surface locked %d", sys->decoder_pictures[sys->decoder_surface_idx].b_lockrect );
    }
    picture->p_sys = &sys->decoder_pictures[sys->decoder_surface_idx++];

#if DEBUG_SURFACE
    msg_Dbg(va, "%lx Extracted pts: %"PRId64" surface %d 0x%p from dxva surface %d 0x%p", GetCurrentThreadId(), picture->date, picture->p_sys->index, picture->p_sys->surface, p_picture_sys->index, source);
#elif 0
    msg_Dbg(va, "%lx Extracted pts: %"PRId64" surface %d 0x%p from dxva surface %d 0x%p", GetCurrentThreadId(), picture->date, picture->p_sys->index, picture->p_sys->surface, p_picture_sys->index, source);
#endif

    /* */
#if GET_DECODER_SURFACE
    IDirect3DSurface9_UnlockRect(source);
    p_picture_sys->b_lockrect = false;
#if DEBUG_SURFACE
    msg_Dbg(va, "d3d surface 0x%p at %d 0x%p unlocked", data, p_picture_sys->index, p_picture_sys );
#endif
#endif
    if ( p_picture_sys->p_lock )
        vlc_mutex_unlock( p_picture_sys->p_lock );

    return VLC_SUCCESS;
}

/* FIXME it is nearly common with VAAPI */
static int Get(vlc_va_t *va, void **opaque, uint8_t **data)
{
    vlc_va_sys_t *sys = va->sys;

    /* Check the device */
    HRESULT hr = IDirect3DDeviceManager9_TestDevice(sys->devmng, sys->device);
    if (hr == DXVA2_E_NEW_VIDEO_DEVICE) {
        if (DxResetVideoDecoder(va))
        {
#if DEBUG_SURFACE
            msg_Dbg( va, "get d3d surface test new device !");
#endif
            return VLC_EGENERIC;
        }
    } else if (FAILED(hr)) {
        msg_Err(va, "IDirect3DDeviceManager9_TestDevice %u", (unsigned)hr);
        return VLC_EGENERIC;
    }

    if ( sys->b_need_thread_safe )
        vlc_mutex_lock( &sys->surface_lock );

    /* Grab an unused surface, in case none are, try the oldest
     * XXX using the oldest is a workaround in case a problem happens with libavcodec */
    unsigned i, old;
    for (i = 0, old = 0; i < sys->surface_count; i++) {
#if GET_DECODER_SURFACE
        picture_sys_t *surface = &sys->decoder_pictures[i];
#else
        picture_sys_t *surface = &sys->surface[i];
#endif

        if ( surface->refcount == 0 && !surface->b_lockrect )
            break;

        if (surface->order < sys->surface[old].order)
            old = i;
    }
    if (i >= sys->surface_count)
    {
        msg_Warn(va, "%lx could not find an unused surface, use oldest index %d", GetCurrentThreadId(), old);
        i = old;
    }
    //i=0;

#if GET_DECODER_SURFACE
    picture_sys_t *p_picture_sys = &sys->decoder_pictures[i];
#else
    picture_sys_t *p_picture_sys = &sys->surface[i];
#endif

#if 0
    LPDIRECT3DSURFACE9 backBufferSurface;
    hr = IDirect3DDevice9_GetBackBuffer( sys->d3ddev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &backBufferSurface );
    if (FAILED(hr)) {
        msg_Err(va, "Failed to get back buffer. (hr=0x%0lx)", hr );
        return VLC_EGENERIC;
    }
    p_picture_sys->surface = backBufferSurface;
#endif

    p_picture_sys->refcount++;
    p_picture_sys->order = sys->surface_order++;
    *data = (void *)p_picture_sys->surface;
    *opaque = p_picture_sys;
#if DEBUG_SURFACE
    msg_Dbg( va, "%lx pool get picture_sys_t 0x%p dxva surface %d 0x%p", GetCurrentThreadId(), p_picture_sys, i, p_picture_sys->surface );
#endif

    IDirect3DSurface9_AddRef( p_picture_sys->surface );

    if ( sys->b_need_thread_safe )
        vlc_mutex_unlock( &sys->surface_lock );

    return VLC_SUCCESS;
}

static void Release(void *opaque, uint8_t *data)
{
    VLC_UNUSED( data );

    picture_sys_t *p_picture = opaque;
    if ( p_picture->p_lock )
        vlc_mutex_lock( p_picture->p_lock );

    p_picture->refcount--;

    IDirect3DSurface9_Release( p_picture->surface );

    if ( p_picture->p_lock )
        vlc_mutex_unlock( p_picture->p_lock );

#if DEBUG_SURFACE
    msg_Dbg( p_picture->va, "%lx pool release picture_t 0x%p dxva surface %d at 0x%p refcount=%d", GetCurrentThreadId(), opaque, p_picture->index, data, p_picture->refcount );
#endif
}

static void Close(vlc_va_t *va, AVCodecContext *ctx)
{
    VLC_UNUSED( ctx );

    vlc_va_sys_t *sys = va->sys;

    DxDestroyVideoDecoder(sys);
    DxDestroyVideoService(sys);
    D3dDestroyDeviceManager(sys);
    D3dDestroyDevice(sys);

    if (sys->hdxva2_dll)
        FreeLibrary(sys->hdxva2_dll);
    if (sys->hd3d9_dll)
        FreeLibrary(sys->hd3d9_dll);
    if ( sys->b_need_thread_safe )
        vlc_mutex_destroy( &sys->surface_lock );

    free((char *)va->description);
    free(sys);
}

static int Open(vlc_va_t *va, AVCodecContext *ctx, const es_format_t *fmt)
{
    VLC_UNUSED( fmt );

    vlc_va_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    va->sys = sys;
#if DEBUG_SURFACE
    sys->va = va;
#endif
    sys->codec_id = ctx->codec_id;
    sys->i_profile = ctx->profile;

    sys->b_need_thread_safe = ctx->thread_safe_callbacks;
    if ( sys->b_need_thread_safe )
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

    /* */
    if (D3dCreateDevice(va)) {
        msg_Err(va, "Failed to create Direct3D device");
        goto error;
    }
    msg_Dbg(va, "D3dCreateDevice succeed");

    if (D3dCreateDeviceManager(va)) {
        msg_Err(va, "D3dCreateDeviceManager failed");
        goto error;
    }

    if (DxCreateVideoService(va)) {
        msg_Err(va, "DxCreateVideoService failed");
        goto error;
    }

    /* */
    if (DxFindVideoServiceConversion(va, &sys->input, &sys->p_render)) {
        msg_Err(va, "DxFindVideoServiceConversion failed");
        goto error;
    }

    sys->thread_count = ctx->thread_count;

    /* TODO print the hardware name/vendor for debugging purposes */
    va->description = DxDescribe(sys);
    va->pix_fmt = PIX_FMT_DXVA2_VLD;
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

    /*
    ** Get the current desktop display mode, so we can set up a back
    ** buffer of the same format
    */
    D3DDISPLAYMODE d3ddm;
    HRESULT hr = IDirect3D9_GetAdapterDisplayMode(sys->d3dobj,
                                                  D3DADAPTER_DEFAULT, &d3ddm);
    if (FAILED(hr)) {
       msg_Err(va, "Could not read adapter display mode. (hr=0x%0lx)", hr);
       return VLC_EGENERIC;
    }

    UINT AdapterToUse = D3DADAPTER_DEFAULT;
    D3DDEVTYPE DeviceType = D3DDEVTYPE_HAL;

#ifndef NDEBUG
    /* TODO copy to dxva2.c */
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
    D3DPRESENT_PARAMETERS *d3dpp = &sys->d3dpp;
    ZeroMemory(d3dpp, sizeof(*d3dpp));
    d3dpp->Flags                  = D3DPRESENTFLAG_VIDEO;
    //d3dpp->Flags                  = D3DPRESENTFLAG_VIDEO | D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    d3dpp->Windowed               = TRUE;
    d3dpp->hDeviceWindow          = NULL;
    d3dpp->SwapEffect             = D3DSWAPEFFECT_DISCARD;
    // D3DSWAPEFFECT_COPY;
    d3dpp->MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp->PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp->BackBufferCount        = 1;
    d3dpp->BackBufferFormat       = d3ddm.Format;
    d3dpp->BackBufferWidth        = __MAX((unsigned int)GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                          d3ddm.Width);
    d3dpp->BackBufferHeight       = __MAX((unsigned int)GetSystemMetrics(SM_CYVIRTUALSCREEN),
                                          d3ddm.Height);
    d3dpp->EnableAutoDepthStencil = FALSE;

    /* Direct3D needs a HWND to create a device, even without using ::Present
    this HWND is used to alert Direct3D when there's a change of focus window.
    For now, use GetDesktopWindow, as it looks harmless */
    LPDIRECT3DDEVICE9 d3ddev;
    if (FAILED(IDirect3D9_CreateDevice(d3dobj, AdapterToUse,
                                       DeviceType, d3dpp->hDeviceWindow,
                                       D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                                       D3DCREATE_MULTITHREADED,
                                       d3dpp, &d3ddev))) {
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
/**
 * Find the best suited decoder mode GUID and render format.
 */
static int DxFindVideoServiceConversion(vlc_va_t *va, GUID *input, const d3d_format_t **output)
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
        if (!mode->codec_id || mode->codec_id != sys->codec_id)
            continue;

        /* */
        bool is_supported = false;
        for (const GUID *g = &input_list[0]; !is_supported && g < &input_list[input_count]; g++) {
            is_supported = IsEqualGUID(mode->guid, g);
        }
        if ( is_supported && mode->i_profile && sys->i_profile > 0)
        {
            if (mode->codec_id == AV_CODEC_ID_H264 &&
                (sys->i_profile & ~FF_PROFILE_H264_CONSTRAINED) > mode->i_profile) {
                msg_Warn( va, "Unsupported H.264 profile for DXVA2 HWAccel: %d", sys->i_profile );
                is_supported = false;
            } else if (sys->i_profile > mode->i_profile) {
                msg_Warn( va, "Unsupported profile for DXVA2 HWAccel: %d", sys->i_profile );
                is_supported = false;
            }
        }
        if (!is_supported)
            continue;

        /* */
        msg_Dbg(va, "Trying to use '%s' as input", mode->name);
        UINT      output_count = 0;
        D3DFORMAT *output_list = NULL;
        if (FAILED(IDirectXVideoDecoderService_GetDecoderRenderTargets(sys->vs, mode->guid,
                                                                       &output_count,
                                                                       &output_list))) {
            msg_Err(va, "IDirectXVideoDecoderService_GetDecoderRenderTargets failed");
            continue;
        }
        for (unsigned j = 0; j < output_count; j++) {
            const D3DFORMAT f = output_list[j];
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
            *output = format;
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
                                const video_format_t *fmt, int i_threading)
{
    vlc_va_sys_t *sys = va->sys;
    int surface_alignment;
    int surface_count;

    /* */
    msg_Dbg(va, "DxCreateVideoDecoder id %d %dx%d",
            codec_id, fmt->i_width, fmt->i_height);

    sys->width  = fmt->i_width;
    sys->height = fmt->i_height;

    /* decoding MPEG-2 requires additional alignment on some Intel GPUs,
       but it causes issues for H.264 on certain AMD GPUs..... */
    if ( codec_id == AV_CODEC_ID_MPEG2VIDEO )
        surface_alignment = 32;
    /* the HEVC DXVA2 spec asks for 128 pixel aligned surfaces to ensure
       all coding features have enough room to work with */
    else if ( codec_id == AV_CODEC_ID_HEVC )
        surface_alignment = 128;
    else
        surface_alignment = 16;

    /* Allocates all surfaces needed for the decoder */
    --surface_alignment;
    sys->surface_width  = (fmt->i_width  + surface_alignment) & ~surface_alignment;
    sys->surface_height = (fmt->i_height + surface_alignment) & ~surface_alignment;

    surface_count = 4;
    /* add surfaces based on number of possible refs */
    if ( codec_id == AV_CODEC_ID_H264 || codec_id == AV_CODEC_ID_HEVC )
        surface_count += 16;
    else
        surface_count += 2;

    if ( i_threading )
        surface_count += 2 * sys->thread_count;

    if (surface_count > VA_DXVA2_MAX_SURFACE_COUNT)
        return VLC_EGENERIC;
    sys->surface_count = surface_count;
    if (FAILED(IDirectXVideoDecoderService_CreateSurface(sys->vs,
                                                         sys->surface_width,
                                                         sys->surface_height,
                                                         sys->surface_count - 1,
                                                         sys->p_render->format,
                                                         D3DPOOL_DEFAULT,
                                                         0,
                                                         DXVA2_VideoDecoderRenderTarget,
                                                         sys->hw_surface,
                                                         NULL))) {
        msg_Err(va, "IDirectXVideoAccelerationService_CreateSurface failed");
        sys->surface_count = 0;
        return VLC_EGENERIC;
    }

    if (FAILED(IDirectXVideoDecoderService_CreateSurface(sys->vs,
                                                         sys->surface_width,
                                                         sys->surface_height,
                                                         sys->surface_count - 1,
                                                         sys->p_render->format,
                                                         D3DPOOL_DEFAULT,
                                                         0,
                                                         DXVA2_VideoDecoderRenderTarget,
                                                         sys->decoder_surface,
                                                         NULL))) {
        msg_Err(va, "IDirectXVideoAccelerationService_CreateSurface decoder failed");
        sys->surface_count = 0;
        return VLC_EGENERIC;
    }
    sys->decoder_surface_num = sys->surface_count;

    for (unsigned i = 0; i < sys->surface_count; i++) {
        picture_sys_t *p_picture = &sys->surface[i];
        p_picture->surface = sys->hw_surface[i];
        p_picture->refcount = 0;
        p_picture->index = i;
        p_picture->order = i;
        p_picture->p_ouput = sys->p_render;
        if ( sys->b_need_thread_safe )
            p_picture->p_lock = &sys->surface_lock;
#if DEBUG_SURFACE
        p_picture->va = va;
        msg_Dbg(va, "init dxva surface object %d 0x%p at 0x%p", i, p_picture, p_picture->surface);
#endif

        picture_sys_t *p_dec_picture = &sys->decoder_pictures[i];
        p_dec_picture->surface = sys->decoder_surface[i];
        p_dec_picture->refcount = 0;
        p_dec_picture->index = i;
        p_dec_picture->order = i;
#if DEBUG_SURFACE
        p_dec_picture->va = va;
        msg_Dbg(va, "init decoder surface object %d 0x%p at 0x%p", i, p_dec_picture, p_dec_picture->surface);
#endif
    }
    msg_Dbg(va, "IDirectXVideoAccelerationService_CreateSurface succeed with %d surfaces (%dx%d)",
            sys->surface_count, fmt->i_width, fmt->i_height);

    /* */
    DXVA2_VideoDesc dsc;
    ZeroMemory(&dsc, sizeof(dsc));
    dsc.SampleWidth     = fmt->i_width;
    dsc.SampleHeight    = fmt->i_height;
    dsc.Format          = sys->p_render->format;
    if (fmt->i_frame_rate > 0 && fmt->i_frame_rate_base > 0) {
        dsc.InputSampleFreq.Numerator   = fmt->i_frame_rate;
        dsc.InputSampleFreq.Denominator = fmt->i_frame_rate_base;
    } else {
        dsc.InputSampleFreq.Numerator   = 0;
        dsc.InputSampleFreq.Denominator = 0;
    }
    dsc.OutputFrameFreq = dsc.InputSampleFreq;
#if 0 // all 0
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
#endif

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
    {
#if DEBUG_SURFACE
        msg_Dbg( va->va, "%lx release dxva surface %d 0x%p", GetCurrentThreadId(), va->surface[i].index, va->hw_surface[i] );
#endif
        IDirect3DSurface9_Release( va->hw_surface[i] );
#if DEBUG_SURFACE
        msg_Dbg( va->va, "%lx release decoder surface %d 0x%p", GetCurrentThreadId(), va->decoder_pictures[i].index, va->decoder_surface[i] );
#endif
        IDirect3DSurface9_Release( va->decoder_surface[i] );
    }
    va->surface_count = 0;
}

static int DxResetVideoDecoder(vlc_va_t *va)
{
    msg_Err(va, "DxResetVideoDecoder unimplemented");
    return VLC_EGENERIC;
}

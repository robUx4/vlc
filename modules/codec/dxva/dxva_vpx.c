/*****************************************************************************
 * atsc_a65.c : ATSC A65 decoding helpers
 *****************************************************************************
 * Copyright (C) 2016 - VideoLAN Authors
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#define COBJMACROS
#define INITGUID
#include <d3d11.h>

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
# include <dxgidebug.h>
#endif

DEFINE_GUID(DXVA_ModeVP8_VLD,                       0x90b899ea, 0x3a62, 0x4705, 0x88, 0xb3, 0x8d, 0xf0, 0x4b, 0x27, 0x44, 0xe7);
#ifndef NDEBUG
DEFINE_GUID(DXVA_ModeVP9_VLD_Profile0/*DXVA2_ModeH264_E*/, 0x1b81be68, 0xa0c7,0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
#else
DEFINE_GUID(DXVA_ModeVP9_VLD_Profile0,              0x463707f8, 0xa1d0, 0x4585, 0x87, 0x6d, 0x83, 0xaa, 0x6d, 0x60, 0xb8, 0x9e);
#endif


/* VPx-specific structures */

/* VPx picture entry data structure */
typedef struct _DXVA_PicEntry_VPx {
    union {
        struct {
            UCHAR Index7Bits : 7;
            UCHAR AssociatedFlag : 1;
        };
        UCHAR bPicEntry;
    };
} DXVA_PicEntry_VPx, *LPDXVA_PicEntry_VPx;

/* VP9 segmentation structure */
typedef struct _segmentation_VP9 {
    union {
        struct {
            UCHAR enabled : 1;
            UCHAR update_map : 1;
            UCHAR temporal_update : 1;
            UCHAR abs_delta : 1;
            UCHAR ReservedSegmentFlags4Bits : 4;
        };
        UCHAR wSegmentInfoFlags;
    };
    UCHAR tree_probs[7];
    UCHAR pred_probs[3];
    SHORT feature_data[8][4];
    UCHAR feature_mask[8];
} DXVA_segmentation_VP9;

/* VP9 picture parameters structure */
typedef struct _DXVA_PicParams_VP9 {
    DXVA_PicEntry_VPx    CurrPic;
    UCHAR                profile;
    union {
        struct {
            USHORT frame_type : 1;
            USHORT show_frame : 1;
            USHORT error_resilient_mode : 1;
            USHORT subsampling_x : 1;
            USHORT subsampling_y : 1;
            USHORT extra_plane : 1;
            USHORT refresh_frame_context : 1;
            USHORT frame_parallel_decoding_mode : 1;
            USHORT intra_only : 1;
            USHORT frame_context_idx : 2;
            USHORT reset_frame_context : 2;
            USHORT allow_high_precision_mv : 1;
            USHORT ReservedFormatInfo2Bits : 2;
        };
        USHORT wFormatAndPictureInfoFlags;
    };
    UINT  width;
    UINT  height;
    UCHAR BitDepthMinus8Luma;
    UCHAR BitDepthMinus8Chroma;
    UCHAR interp_filter;
    UCHAR Reserved8Bits;
    DXVA_PicEntry_VPx  ref_frame_map[8];
    UINT  ref_frame_coded_width[8];
    UINT  ref_frame_coded_height[8];
    DXVA_PicEntry_VPx  frame_refs[3];
    CHAR  ref_frame_sign_bias[4];
    CHAR  filter_level;
    CHAR  sharpness_level;
    union {
        struct {
            UCHAR mode_ref_delta_enabled : 1;
            UCHAR mode_ref_delta_update : 1;
            UCHAR use_prev_in_find_mv_refs : 1;
            UCHAR ReservedControlInfo5Bits : 5;
        };
        UCHAR wControlInfoFlags;
    };
    CHAR   ref_deltas[4];
    CHAR   mode_deltas[2];
    SHORT  base_qindex;
    CHAR   y_dc_delta_q;
    CHAR   uv_dc_delta_q;
    CHAR   uv_ac_delta_q;
    DXVA_segmentation_VP9 stVP9Segments;
    UCHAR  log2_tile_cols;
    UCHAR  log2_tile_rows;
    USHORT uncompressed_header_size_byte_aligned;
    USHORT first_partition_size;
    USHORT Reserved16Bits;
    UINT   Reserved32Bits;
    UINT   StatusReportFeedbackNumber;
} DXVA_PicParams_VP9, *LPDXVA_PicParams_VP9;

/* VP8 segmentation structure */
typedef struct _segmentation_VP8 {
    union {
        struct {
            UCHAR segmentation_enabled : 1;
            UCHAR update_mb_segmentation_map : 1;
            UCHAR update_mb_segmentation_data : 1;
            UCHAR mb_segement_abs_delta : 1;
            UCHAR ReservedSegmentFlags4Bits : 4;
        };
        UCHAR wSegmentFlags;
    };
    CHAR  segment_feature_data[2][4];
    UCHAR mb_segment_tree_probs[3];
} DXVA_segmentation_VP8;

/* VP8 picture parameters structure */
typedef struct _DXVA_PicParams_VP8 {
    UINT first_part_size;
    UINT width;
    UINT height;
    DXVA_PicEntry_VPx  CurrPic;
    union {
        struct {
            UCHAR frame_type : 1;
            UCHAR version : 3;
            UCHAR show_frame : 1;
            UCHAR clamp_type : 1;
            UCHAR ReservedFrameTag3Bits : 2;
        };
        UCHAR wFrameTagFlags;
    };
    DXVA_segmentation_VP8  stVP8Segments;
    UCHAR filter_type;
    UCHAR filter_level;
    UCHAR sharpness_level;
    UCHAR mode_ref_lf_delta_enabled;
    UCHAR mode_ref_lf_delta_update;
    CHAR  ref_lf_deltas[4];
    CHAR  mode_lf_deltas[4];
    UCHAR log2_nbr_of_dct_partitions;
    UCHAR base_qindex;
    CHAR  y1dc_delta_q;
    CHAR  y2dc_delta_q;
    CHAR  y2ac_delta_q;
    CHAR  uvdc_delta_q;
    CHAR  uvac_delta_q;
    DXVA_PicEntry_VPx alt_fb_idx;
    DXVA_PicEntry_VPx gld_fb_idx;
    DXVA_PicEntry_VPx lst_fb_idx;
    UCHAR  ref_frame_sign_bias_golden;
    UCHAR  ref_frame_sign_bias_altref;
    UCHAR  refresh_entropy_probs;
    UCHAR  vp8_coef_update_probs[4][8][3][11];
    UCHAR  mb_no_coeff_skip;
    UCHAR  prob_skip_false;
    UCHAR  prob_intra;
    UCHAR  prob_last;
    UCHAR  prob_golden;
    UCHAR  intra_16x16_prob[4];
    UCHAR  intra_chroma_prob[3];
    UCHAR  vp8_mv_update_probs[2][19];
    USHORT ReservedBits1;
    USHORT ReservedBits2;
    USHORT ReservedBits3;
    UINT   StatusReportFeedbackNumber;
} DXVA_PicParams_VP8, *LPDXVA_PicParams_VP8;

/* VPx slice control data structure - short form */
typedef struct _DXVA_Slice_VPx_Short {
    UINT   BSNALunitDataLocation;
    UINT   SliceBytesInBuffer;
    USHORT wBadSliceChopping;
} DXVA_Slice_VPx_Short, *LPDXVA_Slice_VPx_Short;

/* VPx status reporting data structure */
typedef struct _DXVA_Status_VPx {
    UINT  StatusReportFeedbackNumber;
    DXVA_PicEntry_VPx CurrPic;
    UCHAR  bBufType;
    UCHAR  bStatus;
    UCHAR  bReserved8Bits;
    USHORT wNumMbsAffected;
} DXVA_Status_VPx, *LPDXVA_Status_VPx;

struct decoder_sys_t
{
    /* DLL */
    HINSTANCE             hdecoder_dll;
#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    HINSTANCE             dxgidebug_dll;
#endif

    ID3D11Device          *d3ddev;
    ID3D11DeviceContext   *d3dctx;
    ID3D11VideoContext    *d3dvidctx;
    ID3D11VideoDevice     *d3ddec;
};

static picture_t *Decode( decoder_t *p_dex, block_t **pp_block )
{
    return NULL;
}

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *) p_this;
    decoder_sys_t *sys = p_dec->p_sys;

    if (sys->d3ddec)
        ID3D11VideoDevice_Release(sys->d3ddec);
    if (sys->d3dvidctx)
        ID3D11VideoContext_Release(sys->d3dvidctx);
    if (sys->d3dctx)
        ID3D11DeviceContext_Release(sys->d3dctx);
    if (sys->d3ddev)
        ID3D11Device_Release(sys->d3ddev);
#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    if ( sys->dxgidebug_dll )
        FreeLibrary( sys->dxgidebug_dll );
#endif
    if ( sys->hdecoder_dll )
        FreeLibrary( sys->hdecoder_dll );
    free( sys );
}

static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *) p_this;
    decoder_sys_t *sys = NULL;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_VP8 &&
        p_dec->fmt_in.i_codec != VLC_CODEC_VP9 )
        return VLC_EGENERIC;

    sys = (decoder_sys_t*) calloc( 1, sizeof(decoder_sys_t) );
    if( sys == NULL )
        return VLC_ENOMEM;

    p_dec->p_sys = sys;

    sys->hdecoder_dll = LoadLibrary(TEXT("D3D11.DLL"));
    if (!sys->hdecoder_dll) {
        msg_Warn(p_dec, "cannot load D3D11.dll");
        goto error;
    }

    PFN_D3D11_CREATE_DEVICE pf_CreateDevice;
    pf_CreateDevice = (void *)GetProcAddress(sys->hdecoder_dll, "D3D11CreateDevice");
    if (!pf_CreateDevice) {
        msg_Err(p_dec, "Cannot locate reference to D3D11CreateDevice ABI in DLL");
        goto error;
    }

    UINT creationFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#if !defined(NDEBUG) //&& defined(_MSC_VER)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    /* */
    ID3D11Device *d3ddev;
    ID3D11DeviceContext *d3dctx;
    HRESULT hr = pf_CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                                 creationFlags, NULL, 0,
                                 D3D11_SDK_VERSION, &d3ddev, NULL, &d3dctx);
    if (FAILED(hr)) {
        msg_Err(p_dec, "D3D11CreateDevice failed. (hr=0x%lX)", hr);
        goto error;
    }
    sys->d3ddev = d3ddev;
    sys->d3dctx = d3dctx;

    ID3D11VideoDevice *d3ddec = NULL;
    hr = ID3D11Device_QueryInterface( (ID3D11Device*) sys->d3ddev, &IID_ID3D11VideoDevice, (void **)&d3ddec);
    if (FAILED(hr)) {
       msg_Err(p_dec, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       goto error;
    }
    sys->d3ddec = d3ddec;

    GUID decoderGUID = {0};
    UINT input_count = ID3D11VideoDevice_GetVideoDecoderProfileCount( sys->d3ddec );
    for (unsigned i = 0; i < input_count; i++) {
        GUID guid;
        hr = ID3D11VideoDevice_GetVideoDecoderProfile( sys->d3ddec, i, &guid);
        if (FAILED(hr))
        {
            msg_Err( p_dec, "GetVideoDecoderProfile %d failed. (hr=0x%lX)", i, hr);
            break;
        }
        if ( p_dec->fmt_in.i_codec == VLC_CODEC_VP8 && IsEqualGUID(&guid, &DXVA_ModeVP8_VLD) )
        {
            msg_Dbg(p_dec, "found VP8 hardware decoder");
            decoderGUID = DXVA_ModeVP8_VLD;
            break;
        }
        if ( p_dec->fmt_in.i_codec == VLC_CODEC_VP9 && IsEqualGUID(&guid, &DXVA_ModeVP9_VLD_Profile0) )
        {
            msg_Dbg(p_dec, "found VP9 hardware decoder");
            decoderGUID = DXVA_ModeVP9_VLD_Profile0;
            break;
        }
    }
    if ( decoderGUID.Data1 == 0)
        goto error;

    ID3D11VideoContext *d3dvidctx = NULL;
    hr = ID3D11DeviceContext_QueryInterface(d3dctx, &IID_ID3D11VideoContext, (void **)&d3dvidctx);
    if (FAILED(hr)) {
       msg_Err(p_dec, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       goto error;
    }
    sys->d3dvidctx = d3dvidctx;

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    sys->dxgidebug_dll = LoadLibrary(TEXT("DXGIDEBUG.DLL"));
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

    p_dec->fmt_out.i_cat = VIDEO_ES;
    p_dec->pf_decode_video = Decode;

    return VLC_SUCCESS;

error:
    Close( p_this );
    return VLC_EGENERIC;
}

vlc_module_begin ()
    set_description( N_("DXVA 2.0 VPx decoder") )
    set_shortname( N_("DXVA VPx") )
    set_capability( "decoder", 110 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    set_callbacks( Open, Close )
vlc_module_end ()

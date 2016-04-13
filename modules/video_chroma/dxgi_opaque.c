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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>

#include "copy.h"

#define COBJMACROS
#define INITGUID
#include "dxgi_fmt.h"
#include "d3d11_opaque.h"

static int  OpenConverter( vlc_object_t * );
static void CloseConverter( vlc_object_t * );

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Conversions from D3D11 to YUV") )
    set_capability( "video filter2", 10 )
    set_callbacks( OpenConverter, CloseConverter )
vlc_module_end ()

#include <windows.h>
#define COBJMACROS
#include <d3d11.h>

struct filter_sys_t {
    DXGI_FORMAT                    processor_output;
    ID3D11VideoDevice              *d3ddec;
    ID3D11VideoProcessor           *d3dprocessor;
    ID3D11VideoProcessorEnumerator *d3dprocenum;

    ID3D11VideoContext             *d3dvidctx;
    //copy_cache_t     cache;
    //ID3D11Texture2D  *staging;
    //vlc_mutex_t      staging_lock;
};

static void DXGI_D3D11(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = (filter_sys_t*) p_filter->p_sys;
    HRESULT hr;
    picture_sys_t *p_sys_in  = src->p_sys;
    picture_sys_t *p_sys_out = dst->p_sys;

    // extract the decoded video to a the output Texture
    if (p_sys_out->decoder == NULL)
    {
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc = {
            .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
        };

        HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorOutputView( sys->d3ddec,
                                                                       (ID3D11Resource*) p_sys_out->texture,
                                                                       sys->d3dprocenum,
                                                                       &outDesc,
                                                                       (ID3D11VideoProcessorOutputView**) &p_sys_out->decoder );
        if (FAILED(hr))
        {
            msg_Err( p_filter, "Failed to create the processor output. (hr=0x%lX)", hr);
            return;
        }
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream = {
        .Enable = TRUE,
        .pInputSurface = p_sys_in->inputView,
    };

    hr = ID3D11VideoContext_VideoProcessorBlt(sys->d3dvidctx, sys->d3dprocessor,
                                              (ID3D11VideoProcessorOutputView*) p_sys_out->decoder,
                                              0, 1, &stream);
    if (FAILED(hr))
    {
        msg_Err( p_filter, "Failed to process the video. (hr=0x%lX)", hr);
        return;
    }
}

#if 0
static int assert_staging(filter_t *p_filter, picture_sys_t *p_sys)
{
    filter_sys_t *sys = (filter_sys_t*) p_filter->p_sys;
    HRESULT hr;

    if (sys->staging)
        goto ok;

    D3D11_TEXTURE2D_DESC texDesc;
    ID3D11Texture2D_GetDesc( p_sys->texture, &texDesc);

    texDesc.MipLevels = 1;
    //texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0;
    texDesc.ArraySize = 1;
    texDesc.Usage = D3D11_USAGE_STAGING;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    texDesc.BindFlags = 0;

    ID3D11Device *p_device;
    ID3D11DeviceContext_GetDevice(p_sys->context, &p_device);
    sys->staging = NULL;
    hr = ID3D11Device_CreateTexture2D( p_device, &texDesc, NULL, &sys->staging);
    ID3D11Device_Release(p_device);
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to create a %s staging texture to extract surface pixels (hr=0x%0lx)", DxgiFormatToStr(texDesc.Format), hr );
        return VLC_EGENERIC;
    }
ok:
    return VLC_SUCCESS;
}

static void D3D11_YUY2(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = (filter_sys_t*) p_filter->p_sys;
    picture_sys_t *p_sys = src->p_sys;

    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE lock;

    vlc_mutex_lock(&sys->staging_lock);
    if (assert_staging(p_filter, p_sys) != VLC_SUCCESS)
    {
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    ID3D11VideoDecoderOutputView_GetDesc( p_sys->decoder, &viewDesc );

    ID3D11DeviceContext_CopySubresourceRegion(p_sys->context, (ID3D11Resource*) sys->staging,
                                              0, 0, 0, 0,
                                              (ID3D11Resource*) p_sys->texture, viewDesc.Texture2D.ArraySlice,
                                              NULL);

    HRESULT hr = ID3D11DeviceContext_Map(p_sys->context, (ID3D11Resource*) sys->staging,
                                         0, D3D11_MAP_READ, 0, &lock);
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to map source surface. (hr=0x%0lx)", hr);
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    ID3D11Texture2D_GetDesc(sys->staging, &desc);

    if (desc.Format == DXGI_FORMAT_YUY2) {
        size_t chroma_pitch = (lock.RowPitch / 2);

        size_t pitch[3] = {
            lock.RowPitch,
            chroma_pitch,
            chroma_pitch,
        };

        uint8_t *plane[3] = {
            (uint8_t*)lock.pData,
            (uint8_t*)lock.pData + pitch[0] * src->format.i_height,
            (uint8_t*)lock.pData + pitch[0] * src->format.i_height
                                 + pitch[1] * src->format.i_height / 2,
        };

        CopyFromYv12(dst, plane, pitch, src->format.i_width,
                     src->format.i_height, &sys->cache);
    } else if (desc.Format == DXGI_FORMAT_NV12) {
        uint8_t *plane[2] = {
            lock.pData,
            (uint8_t*)lock.pData + lock.RowPitch * src->format.i_height
        };
        size_t  pitch[2] = {
            lock.RowPitch,
            lock.RowPitch,
        };
        CopyFromNv12(dst, plane, pitch, src->format.i_width,
                     src->format.i_height, &sys->cache);
    } else {
        msg_Err(p_filter, "Unsupported D3D11VA conversion from 0x%08X to YV12", desc.Format);
    }

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    /* */
    ID3D11DeviceContext_Unmap(p_sys->context, (ID3D11Resource*)sys->staging, 0);
    vlc_mutex_unlock(&sys->staging_lock);
}

static void D3D11_NV12(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = (filter_sys_t*) p_filter->p_sys;
    picture_sys_t *p_sys = src->p_sys;

    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE lock;

    vlc_mutex_lock(&sys->staging_lock);
    if (assert_staging(p_filter, p_sys) != VLC_SUCCESS)
    {
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    ID3D11VideoDecoderOutputView_GetDesc( p_sys->decoder, &viewDesc );

    ID3D11DeviceContext_CopySubresourceRegion(p_sys->context, (ID3D11Resource*) sys->staging,
                                              0, 0, 0, 0,
                                              (ID3D11Resource*) p_sys->texture, viewDesc.Texture2D.ArraySlice,
                                              NULL);

    HRESULT hr = ID3D11DeviceContext_Map(p_sys->context, (ID3D11Resource*) sys->staging,
                                         0, D3D11_MAP_READ, 0, &lock);
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to map source surface. (hr=0x%0lx)", hr);
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    ID3D11Texture2D_GetDesc(sys->staging, &desc);

    if (desc.Format == DXGI_FORMAT_NV12) {
        uint8_t *plane[2] = {
            lock.pData,
            (uint8_t*)lock.pData + lock.RowPitch * src->format.i_height
        };
        size_t  pitch[2] = {
            lock.RowPitch,
            lock.RowPitch,
        };
        CopyFromNv12ToNv12(dst, plane, pitch, src->format.i_width,
                           src->format.i_height, &sys->cache);
    } else {
        msg_Err(p_filter, "Unsupported D3D11VA conversion from 0x%08X to NV12", desc.Format);
    }

    /* */
    ID3D11DeviceContext_Unmap(p_sys->context, (ID3D11Resource*)sys->staging, 0);
    vlc_mutex_unlock(&sys->staging_lock);
}

VIDEO_FILTER_WRAPPER (D3D11_NV12)
VIDEO_FILTER_WRAPPER (D3D11_YUY2)
#endif
VIDEO_FILTER_WRAPPER (DXGI_D3D11)

static int OpenConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    if ( p_filter->fmt_in.video.i_chroma != VLC_CODEC_DXGI_OPAQUE )
        return VLC_EGENERIC;
    if ( p_filter->fmt_out.video.i_chroma != VLC_CODEC_D3D11_OPAQUE )
        return VLC_EGENERIC;

    if ( p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
         || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width )
        return VLC_EGENERIC;

    pool_picture_factory *pool_factory_out = pool_HandlerGetFactory( p_filter->p_pool_handler,
                                                                   p_filter->fmt_out.video.i_chroma,
                                                                   p_filter->fmt_out.video.p_sub_chroma,
                                                                   false,
                                                                   true );
    if ( pool_factory_out == NULL )
    {
        return VLC_EGENERIC;
    }
    pool_factory_d3d11 *pool_d3d11_out = pool_factory_out->p_opaque;

    pool_picture_factory *pool_factory_in = pool_HandlerGetFactory( p_filter->p_pool_handler,
                                                                   p_filter->fmt_in.video.i_chroma,
                                                                   p_filter->fmt_in.video.p_sub_chroma,
                                                                   false,
                                                                   true );
    if ( pool_factory_in == NULL )
    {
        return VLC_EGENERIC;
    }
    pool_factory_d3d11 *pool_d3d11_in = pool_factory_in->p_opaque;

    filter_sys_t *p_sys = calloc(1, sizeof(filter_sys_t));
    if (!p_sys)
         return VLC_ENOMEM;

    ID3D11VideoDevice *d3dviddev = NULL;
    HRESULT hr = ID3D11Device_QueryInterface( pool_d3d11_in->d3ddevice, &IID_ID3D11VideoDevice, (void **)&d3dviddev);
    if (FAILED(hr)) {
       msg_Err( obj, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    p_sys->d3ddec = d3dviddev;

    ID3D11VideoContext *d3dvidctx = NULL;
    hr = ID3D11DeviceContext_QueryInterface( pool_d3d11_in->d3dcontext, &IID_ID3D11VideoContext, (void **)&d3dvidctx);
    if (FAILED(hr)) {
       msg_Err( obj, "Could not Query ID3D11VideoDevice Interface from the picture. (hr=0x%lX)", hr);
       ID3D11VideoDevice_Release( p_sys->d3ddec );
       return VLC_EGENERIC;
    }
    p_sys->d3dvidctx = d3dvidctx;

    ID3D11VideoProcessorEnumerator *processorEnumerator;
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC processorDesc = {
       .InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,   /* TODO */
       .InputFrameRate = {
           .Numerator   = p_filter->fmt_in.video.i_frame_rate,
           .Denominator = p_filter->fmt_in.video.i_frame_rate_base,
       },
       .InputWidth   = p_filter->fmt_in.video.i_width,
       .InputHeight  = p_filter->fmt_in.video.i_height,
       .OutputWidth  = p_filter->fmt_in.video.i_width,
       .OutputHeight = p_filter->fmt_in.video.i_height,
       .Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL,
    };
    hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator( p_sys->d3ddec, &processorDesc, &processorEnumerator);
    if ( processorEnumerator == NULL )
    {
        msg_Dbg( obj, "Can't get a video processor for the video.");
        ID3D11VideoDevice_Release( p_sys->d3ddec );
        ID3D11DeviceContext_Release( p_sys->d3dvidctx );
        return VLC_EGENERIC;
    }

    UINT flags;
#ifndef NDEBUG
    for (int format = 0; format < 188; format++) {
       hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(processorEnumerator, format, &flags);
       if (SUCCEEDED(hr) && (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT))
           msg_Dbg( obj, "processor format %s is supported for input", DxgiFormatToStr(format));
       if (SUCCEEDED(hr) && (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
           msg_Dbg( obj, "processor format %s is supported for output", DxgiFormatToStr(format));
    }
#endif
    DXGI_FORMAT processorOutput = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT testOutput = p_filter->fmt_out.video.p_sub_chroma->textureFormat;
    if ( testOutput != DXGI_FORMAT_UNKNOWN )
    {
       hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(processorEnumerator, testOutput, &flags);
       if (FAILED(hr) && !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
          msg_Dbg( obj, "processor format %s not supported for output", DxgiFormatToStr(testOutput));
       else
           processorOutput = testOutput;
    }

#if 0
    if (processorOutput == DXGI_FORMAT_UNKNOWN)
    {
       // check if we can create render texture of that format
       // check the decoder can output to that format
       const UINT i_quadSupportFlags = D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_LOAD;
       for (const d3d_format_t *output = GetRenderFormatList();
            output->name != NULL; ++output)
       {
           UINT i_formatSupport;
           if( SUCCEEDED( ID3D11Device_CheckFormatSupport( pool_d3d11_in->d3ddevice,
                                                          output->formatTexture,
                                                          &i_formatSupport)) &&
                   ( i_formatSupport & i_quadSupportFlags ) == i_quadSupportFlags )
           {
               msg_Dbg( obj, "Render pixel format %s supported", DxgiFormatToStr(output->formatTexture) );

               hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(processorEnumerator, output->formatTexture, &flags);
               if (FAILED(hr) && !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
                   msg_Dbg( obj, "Processor format %s not supported for output", DxgiFormatToStr(output->formatTexture));
               else
               {
                   processorOutput = output->formatTexture;
                   break;
               }
           }
       }
    }
#endif

#if 1
    if (processorOutput != DXGI_FORMAT_UNKNOWN)
    {
       D3D11_VIDEO_PROCESSOR_CAPS processorCaps;

       hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(processorEnumerator, &processorCaps);

       ID3D11VideoProcessor *opaque_processor = NULL;
       for (UINT type = 0; type < processorCaps.RateConversionCapsCount; ++type)
       {
           hr = ID3D11VideoDevice_CreateVideoProcessor(p_sys->d3ddec, processorEnumerator, type, &opaque_processor);
           if (SUCCEEDED(hr))
               break;
           opaque_processor = NULL;
       }

       if (opaque_processor != NULL)
       {
           msg_Dbg( obj, "Using processor %s to %s", DxgiFormatToStr(DXGI_FORMAT_420_OPAQUE), DxgiFormatToStr(processorOutput));

           p_sys->d3dprocessor     = opaque_processor;
           p_sys->processor_output = processorOutput;
           //p_sys->render           = DXGI_FORMAT_420_OPAQUE;
           p_sys->d3dprocenum      = processorEnumerator;
           //free(psz_decoder_name);
           //return VLC_SUCCESS;
       }
    }
#endif
    ID3D11VideoProcessorEnumerator_Release(processorEnumerator);

    p_filter->pf_video_filter = DXGI_D3D11_Filter;
#if 0
    switch( p_filter->fmt_out.video.i_chroma ) {
    case VLC_CODEC_I420:
    case VLC_CODEC_YV12:
        p_filter->pf_video_filter = D3D11_YUY2_Filter;
        break;
    case VLC_CODEC_NV12:
        p_filter->pf_video_filter = D3D11_NV12_Filter;
        break;
    default:
        return VLC_EGENERIC;
    }

    CopyInitCache(&p_sys->cache, p_filter->fmt_in.video.i_width );
    vlc_mutex_init(&p_sys->staging_lock);
#endif
    p_filter->p_sys = p_sys;

    return VLC_SUCCESS;
}

static void CloseConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = (filter_sys_t*) p_filter->p_sys;
    if ( p_sys->d3dprocenum )
         ID3D11VideoProcessorEnumerator_Release( p_sys->d3dprocenum );
    if ( p_sys->d3dvidctx )
        ID3D11DeviceContext_Release( p_sys->d3dvidctx );
    if ( p_sys->d3dprocessor )
        ID3D11VideoProcessor_Release( p_sys->d3dprocessor );
    if ( p_sys->d3ddec )
        ID3D11VideoDevice_Release( p_sys->d3ddec );
    free( p_sys );
    p_filter->p_sys = NULL;
}

/*****************************************************************************
 * canvas.c : automatically resize and padd a video to fit in canvas
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
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

#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate( vlc_object_t * );
static void Destroy( vlc_object_t * );
static picture_t *Filter( filter_t *, picture_t * );

/* This module effectively implements a form of picture-in-picture.
 *  - The outer picture is called the canvas.
 *  - The innter picture is callsed the subpicture.
 *
 * NB, all of the following operatons take into account aspect ratio
 *
 * A canvas is of canvas_{width,height}.
 * In Pad mode:
 *  - The subpicture is upconverted with a inverse scalefactor of:
 *     (The size of subpicture's largest dimension)
 *     --------------------------------------------
 *     (The size of canvas's equivalent dimension)
 *
 *   Ie, The subpicture's largest dimension is made equal to the
 *   equivalent canvas dimension.
 *
 *  - The upconverted subpicture's smallest dimension is then padded
 *    to make the upconverted subpicture have the same dimensions of
 *    the canvas.
 *
 * In Crop mode:
 *  - The subpicture is upconverted with an inverse scalefactor of:
 *     (The size of subpicture's smallest dimension)
 *     --------------------------------------------
 *     (The size of canvas's equivalent dimension)
 *
 *   Ie, The subpicture's smallest dimension is made equal to the
 *   equivalent canvas dimension. (The subpicture will then be
 *   larger than the canvas)
 *
 *  - The upconverted subpicture's largest dimension is then cropped
 *    to make the upconverted subpicture have the same dimensions of
 *    the canvas.
 */

/* NB, use of `padd' in this module is a 16-17th Century spelling of `pad' */

#define WIDTH_TEXT N_( "Output width" )
#define WIDTH_LONGTEXT N_( \
    "Output (canvas) image width" )
#define HEIGHT_TEXT N_( "Output height" )
#define HEIGHT_LONGTEXT N_( \
    "Output (canvas) image height" )
#define ASPECT_TEXT N_( "Output picture aspect ratio" )
#define ASPECT_LONGTEXT N_( \
    "Set the canvas' picture aspect ratio. " \
    "If omitted, the canvas is assumed to have the same SAR as the input." )
#define PADD_TEXT N_( "Pad video" )
#define PADD_LONGTEXT N_( \
    "If enabled, video will be padded to fit in canvas after scaling. " \
    "Otherwise, video will be cropped to fix in canvas after scaling." )
#define CANVAS_HELP N_( "Automatically resize and pad a video" )

#define CFG_PREFIX "canvas-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( N_("Canvas") )
    set_description( N_("Canvas video filter") )
    set_capability( "video filter", 0 )
    set_help( CANVAS_HELP )
    set_callbacks( Activate, Destroy )

    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_integer_with_range( CFG_PREFIX "width", 0, 0, INT_MAX,
                            WIDTH_TEXT, WIDTH_LONGTEXT, false )
    add_integer_with_range( CFG_PREFIX "height", 0, 0, INT_MAX,
                            HEIGHT_TEXT, HEIGHT_LONGTEXT, false )

    add_string( CFG_PREFIX "aspect", NULL,
                ASPECT_TEXT, ASPECT_LONGTEXT, false )

    add_bool( CFG_PREFIX "padd", true,
              PADD_TEXT, PADD_LONGTEXT, false )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "width", "height", "aspect", "padd", NULL
};

struct filter_sys_t
{
    filter_chain_t *p_chain;
};

static picture_t *video_new( filter_t *p_filter )
{
    return filter_NewPicture( p_filter->owner.sys );
}

/*****************************************************************************
 *
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    unsigned i_canvas_width; /* visible width of output canvas */
    unsigned i_canvas_height; /* visible height of output canvas */
    unsigned i_canvas_aspect; /* canvas PictureAspectRatio */
    es_format_t fmt; /* target format after up/down conversion */
    char psz_croppadd[100];
    int i_padd,i_offset;
    char *psz_aspect;
    bool b_padd;
    unsigned i_fmt_in_aspect;

    if( !p_filter->b_allow_fmt_out_change )
    {
        msg_Err( p_filter, "Picture format change isn't allowed" );
        return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        msg_Err( p_filter, "Input and output chromas don't match" );
        return VLC_EGENERIC;
    }

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    i_canvas_width = var_CreateGetInteger( p_filter, CFG_PREFIX "width" );
    i_canvas_height = var_CreateGetInteger( p_filter, CFG_PREFIX "height" );

    if( i_canvas_width == 0 || i_canvas_height == 0 )
    {
        msg_Err( p_filter, "Width and height options must be set" );
        return VLC_EGENERIC;
    }

    if( i_canvas_width & 1 || i_canvas_height & 1 )
    {
        /* If this restriction were ever relaxed, it is very important to
         * get the field polatiry correct */
        msg_Err( p_filter, "Width and height options must be even integers" );
        return VLC_EGENERIC;
    }

    if( es_format_HasValidSar( &p_filter->fmt_in ) )
        i_fmt_in_aspect = (uint64_t)p_filter->fmt_in.video.sar.num *
                      p_filter->fmt_in.video.i_visible_width *
                      VOUT_ASPECT_FACTOR /
                      (p_filter->fmt_in.video.sar.den *
                      p_filter->fmt_in.video.i_visible_height);
    else
        i_fmt_in_aspect = (int64_t)p_filter->fmt_in.video.i_visible_width *
                      VOUT_ASPECT_FACTOR /
                      p_filter->fmt_in.video.i_visible_height;

    psz_aspect = var_CreateGetNonEmptyString( p_filter, CFG_PREFIX "aspect" );
    if( psz_aspect )
    {
        vlc_urational_t aspect;
        if( sscanf( psz_aspect, "%u:%u", &aspect.num, &aspect.den ) != 2 ||
            !vlc_valid_aspect_ratio( &aspect ) )
        {
            msg_Err( p_filter, "Invalid aspect ratio" );
            return VLC_EGENERIC;
        }
        i_canvas_aspect = aspect.num * VOUT_ASPECT_FACTOR / aspect.den;
        free( psz_aspect );
    }
    else
    {
        /* if there is no user supplied aspect ratio, assume the canvas
         * has the same sample aspect ratio as the subpicture */
        /* aspect = subpic_sar * canvas_width / canvas_height
         *  where subpic_sar = subpic_ph * subpic_par / subpic_pw */
        i_canvas_aspect = (uint64_t) p_filter->fmt_in.video.i_visible_height
                        * i_fmt_in_aspect
                        * i_canvas_width
                        / (i_canvas_height * p_filter->fmt_in.video.i_visible_width);
    }

    b_padd = var_CreateGetBool( p_filter, CFG_PREFIX "padd" );

    filter_sys_t *p_sys = (filter_sys_t *)malloc( sizeof( filter_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_filter->p_sys = p_sys;

    filter_owner_t owner = {
        .sys = p_filter,
        .video = {
            .buffer_new = video_new,
        },
    };

    p_sys->p_chain = filter_chain_NewVideo( p_filter, true, &owner );
    if( !p_sys->p_chain )
    {
        msg_Err( p_filter, "Could not allocate filter chain" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    es_format_Copy( &fmt, &p_filter->fmt_in );

    /* one dimension will end up with one of the following: */
    fmt.video.i_visible_width = i_canvas_width;
    fmt.video.i_visible_height = i_canvas_height;

    if( b_padd )
    {
        /* Padd */
        if( i_canvas_aspect > i_fmt_in_aspect )
        {
            /* The canvas has a wider aspect than the subpicture:
             *  ie, pillarbox the [scaled] subpicture */
            /* The following is derived form:
             * width = upconverted_subpic_height * subpic_par / canvas_sar
             *  where canvas_sar = canvas_width / (canvas_height * canvas_par)
             * then simplify */
            fmt.video.i_visible_width = i_canvas_width
                              * i_fmt_in_aspect
                              / i_canvas_aspect;
            if( fmt.video.i_visible_width & 1 ) fmt.video.i_visible_width -= 1;

            i_padd = (i_canvas_width - fmt.video.i_visible_width) / 2;
            i_offset = (i_padd & 1);
            snprintf( psz_croppadd, 100, "croppadd{paddleft=%d,paddright=%d}",
                      i_padd - i_offset, i_padd + i_offset );
        }
        else
        {
            /* The canvas has a taller aspect than the subpicture:
             *  ie, letterbox the [scaled] subpicture */
            fmt.video.i_visible_height = i_canvas_height
                               * i_canvas_aspect
                               / i_fmt_in_aspect;
            if( fmt.video.i_visible_height & 1 ) fmt.video.i_visible_height -= 1;

            i_padd = (i_canvas_height - fmt.video.i_visible_height ) / 2;
            i_offset = (i_padd & 1);
            snprintf( psz_croppadd, 100, "croppadd{paddtop=%d,paddbottom=%d}",
                      i_padd - i_offset, i_padd + i_offset );
        }
    }
    else
    {
        /* Crop */
        if( i_canvas_aspect < i_fmt_in_aspect )
        {
            /* The canvas has a narrower aspect than the subpicture:
             *  ie, crop the [scaled] subpicture horizontally */
            fmt.video.i_visible_width = i_canvas_width
                              * i_fmt_in_aspect
                              / i_canvas_aspect;
            if( fmt.video.i_visible_width & 1 ) fmt.video.i_visible_width -= 1;

            i_padd = (fmt.video.i_visible_width - i_canvas_width) / 2;
            i_offset = (i_padd & 1);
            snprintf( psz_croppadd, 100, "croppadd{cropleft=%d,cropright=%d}",
                      i_padd - i_offset, i_padd + i_offset );
        }
        else
        {
            /* The canvas has a shorter aspect than the subpicture:
             *  ie, crop the [scaled] subpicture vertically */
            fmt.video.i_visible_height = i_canvas_height
                               * i_canvas_aspect
                               / i_fmt_in_aspect;
            if( fmt.video.i_visible_height & 1 ) fmt.video.i_visible_height -= 1;

            i_padd = (fmt.video.i_visible_height - i_canvas_height) / 2;
            i_offset = (i_padd & 1);
            snprintf( psz_croppadd, 100, "croppadd{croptop=%d,cropbottom=%d}",
                      i_padd - i_offset, i_padd + i_offset );
        }
    }

    /* xxx, should the clean area include the letter-boxing?
     *  probably not, as some codecs can make use of that information
     *  and it should be a scaled version of the input clean area
     *   -- davidf */
    fmt.video.i_width = p_filter->fmt_in.video.i_width * fmt.video.i_visible_width / p_filter->fmt_in.video.i_visible_width;
    fmt.video.i_height = p_filter->fmt_in.video.i_height * fmt.video.i_visible_height / p_filter->fmt_in.video.i_visible_height;

    filter_chain_Reset( p_sys->p_chain, &p_filter->fmt_in, &fmt );
    /* Append scaling module */
    if ( !filter_chain_AppendConverter( p_sys->p_chain, NULL, NULL ) )
    {
        msg_Err( p_filter, "Could not append scaling filter" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Append croppadd module if we actually do cropping or padding instead of just scaling*/
    if( i_padd > 0 )
    {
        if ( !filter_chain_AppendFromString( p_sys->p_chain, psz_croppadd ) )
        {
            msg_Err( p_filter, "Could not append cropadd filter" );
            filter_chain_Delete( p_sys->p_chain );
            free( p_sys );
            return VLC_EGENERIC;
        }
    }

    fmt = *filter_chain_GetFmtOut( p_sys->p_chain );
    es_format_Copy( &p_filter->fmt_out, &fmt );

    vlc_ureduce( &p_filter->fmt_out.video.sar,
        i_canvas_aspect    * p_filter->fmt_out.video.i_visible_height,
        VOUT_ASPECT_FACTOR * p_filter->fmt_out.video.i_visible_width,
        0);

    if( p_filter->fmt_out.video.i_visible_width != i_canvas_width
     || p_filter->fmt_out.video.i_visible_height != i_canvas_height )
    {
        msg_Warn( p_filter, "Looks like something went wrong. "
                  "Output size is %dx%d while we asked for %dx%d",
                  p_filter->fmt_out.video.i_visible_width,
                  p_filter->fmt_out.video.i_visible_height,
                  i_canvas_width, i_canvas_height );
    }

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_chain_Delete( p_filter->p_sys->p_chain );
    free( p_filter->p_sys );
}

/*****************************************************************************
 *
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    return filter_chain_VideoFilter( p_filter->p_sys->p_chain, p_pic );
}

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

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *) p_this;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_VP8 &&
        p_dec->fmt_in.i_codec != VLC_CODEC_VP9 )
    {
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void Close( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("DXVA 2.0 VPx decoder") )
    set_shortname( N_("DXVA VPx") )
    set_capability( "decoder", 50 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    set_callbacks( Open, Close )
vlc_module_end ()

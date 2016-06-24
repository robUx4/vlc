/*****************************************************************************
 * dummyfilter.c : Pseudo demuxer filter module for vlc
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

static int Demux( demux_t *p_demux )
{
    return demux_Demux( p_demux->p_next );
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return demux_vaControl( p_demux->p_next, i_query, args );
}

static int Open( vlc_object_t * p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    return VLC_SUCCESS;
}

static void Close( vlc_object_t * p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_Delete(p_demux->p_next);
}

vlc_module_begin ()
    set_shortname( "Dummy demux filter" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_( "Dummy demuxer filter" ) )
    set_capability( "demux_filter", 0 )
    add_shortcut( "dummy_demux" )
    set_callbacks( Open, Close )
vlc_module_end ()

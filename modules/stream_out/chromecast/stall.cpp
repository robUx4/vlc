/*****************************************************************************
 * cast.cpp: Chromecast sout module for vlc
 *****************************************************************************
 * Copyright © 2016 VLC authors and VideoLAN
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
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

#include "chromecast_common.h"
#include <vlc_modules.h>
#include <vlc_plugin.h>

#include <vlc_sout.h>

#include <cassert>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()

    set_shortname(N_("cc_stall"))
    set_description(N_("Chromecast stream output stalling"))
    set_capability("sout stream", 0)
    add_shortcut("cc_preload")
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_STREAM)
    set_callbacks(Open, Close)

vlc_module_end ()

static sout_stream_id_sys_t *Add(sout_stream_t *p_stream, const es_format_t *p_fmt)
{
    return sout_StreamIdAdd( p_stream->p_next, p_fmt );
}

static void Del(sout_stream_t *p_stream, sout_stream_id_sys_t *id)
{
    sout_StreamIdDel( p_stream->p_next, id );
}

static int Send(sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                block_t *p_buffer)
{
    /* TODO don't send the LOAD command until this has been called once */
    /* just notify the chromecast_ctrl that sending is happening, it will handle
     *  its internal state for new files/restart/etc */
    /* TODO only start sending when the Chromecast starts to load */
    /* TODO we need to pass the header block at first */
    msg_Dbg( p_stream, "sending data" );
    return sout_StreamIdSend( p_stream->p_next, id, p_buffer );
}

static void Flush( sout_stream_t *p_stream, sout_stream_id_sys_t *id )
{
    sout_StreamFlush( p_stream->p_next, id );
}

static int Control(sout_stream_t *p_stream, int i_query, va_list args)
{
    return p_stream->p_next->pf_control( p_stream->p_next, i_query, args );
}

/*****************************************************************************
 * Open: connect to the Chromecast and initialize the sout
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    sout_stream_t *p_stream = reinterpret_cast<sout_stream_t*>(p_this);

    // Set the sout callbacks.
    p_stream->pf_add     = Add;
    p_stream->pf_del     = Del;
    p_stream->pf_send    = Send;
    p_stream->pf_flush   = Flush;
    p_stream->pf_control = Control;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    sout_stream_t *p_stream = reinterpret_cast<sout_stream_t*>(p_this);

    delete p_stream->p_sys;
}


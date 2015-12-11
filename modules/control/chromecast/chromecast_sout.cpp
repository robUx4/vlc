/*****************************************************************************
 * chromecast.cpp: Chromecast sout module for vlc
 *****************************************************************************
 * Copyright © 2015 VideoLAN
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

#include "chromecast.h"

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>

#include <cassert>
#include <sstream>

static int SoutOpen(vlc_object_t *);
static void SoutClose(vlc_object_t *);

#define SOUT_CFG_PREFIX    "sout-chromecast-"

/* sout wrapper that can tell when the Chromecast is finished playing
 * rather than when data are finished sending */
vlc_module_begin ()
    set_shortname( "cc_sout" )
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_STREAM)
    set_description( N_( "chromecast sout wrapper" ) )
    set_capability("sout stream", 0)
    add_shortcut("cc_sout")
    set_callbacks( SoutOpen, SoutClose )
vlc_module_end ()

struct sout_stream_sys_t
{
    sout_stream_sys_t(sout_stream_t *stream, intf_thread_t *intf, sout_stream_t *sout)
        :p_wrapped(sout)
        ,p_intf(intf)
        ,p_stream(stream)
        ,b_header_started(false)
    {
        assert(p_intf != NULL);
        vlc_object_hold(p_intf);
    }

    ~sout_stream_sys_t()
    {
        sout_StreamChainDelete(p_wrapped, p_wrapped);
        vlc_object_release(p_intf);
    }

    bool isFinishedPlaying() const {
        /* check if the Chromecast to be done playing */
        return p_intf->p_sys->isFinishedPlaying();
    }

    sout_stream_t * const p_wrapped;
    intf_thread_t * const p_intf;

protected:
    sout_stream_t * const p_stream;
    bool                  b_header_started;
};

/*****************************************************************************
 * Sout callbacks
 *****************************************************************************/
static sout_stream_id_sys_t *Add(sout_stream_t *p_stream, const es_format_t *p_fmt)
{
    if (p_stream->p_sys->p_intf->p_sys->canDisplay == AUDIO_ONLY)
    {
        if (p_fmt->i_cat != AUDIO_ES)
            return NULL;
    }
    return sout_StreamIdAdd( p_stream->p_sys->p_wrapped, p_fmt );
}

static void Del(sout_stream_t *p_stream, sout_stream_id_sys_t *id)
{
    sout_StreamIdDel( p_stream->p_sys->p_wrapped, id );
}

static int Send(sout_stream_t *p_stream, sout_stream_id_sys_t *id, block_t *p_buffer)
{
    return sout_StreamIdSend( p_stream->p_sys->p_wrapped, id, p_buffer );
}

static int Flush( sout_stream_t *p_stream, sout_stream_id_sys_t *id )
{
    return sout_StreamFlush( p_stream->p_sys->p_wrapped, id );
}

static int Control(sout_stream_t *p_stream, int i_query, va_list args)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if (i_query == SOUT_STREAM_EMPTY)
    {
        bool *b = va_arg( args, bool * );
        *b = p_sys->isFinishedPlaying();
        return VLC_SUCCESS;
    }

    return p_sys->p_wrapped->pf_control( p_sys->p_wrapped, i_query, args );
}

int SoutOpen(vlc_object_t *p_this)
{
    sout_stream_t *p_stream = reinterpret_cast<sout_stream_t*>(p_this);
    char *psz_var_mux = NULL, *psz_var_mime = NULL;
    intf_thread_t *p_intf = NULL;
    sout_stream_sys_t *p_sys = NULL;
    sout_stream_t *p_sout = NULL;
    std::stringstream ss;

    p_intf = static_cast<intf_thread_t*>(var_InheritAddress(p_stream, SOUT_INTF_ADDRESS));
    if (p_intf == NULL) {
        msg_Err(p_stream, "Missing the control interface to work");
        goto error;
    }

    psz_var_mux = var_InheritString(p_stream, SOUT_CFG_PREFIX "mux");
    if (psz_var_mux == NULL || !psz_var_mux[0])
        goto error;
    psz_var_mime = var_InheritString(p_stream, SOUT_CFG_PREFIX "mime");
    if (psz_var_mime == NULL || !psz_var_mime[0])
        goto error;

    ss << "http{dst=:" << var_InheritInteger(p_stream, SOUT_CFG_PREFIX "http-port") << "/stream"
       << ",mux=" << psz_var_mux
       << ",access=http{mime=" << psz_var_mime << "}}";

    p_sout = sout_StreamChainNew( p_stream->p_sout, ss.str().c_str(), NULL, NULL);
    if (p_sout == NULL) {
        msg_Dbg(p_stream, "could not create sout chain:%s", ss.str().c_str());
        goto error;
    }

    p_sys = new(std::nothrow) sout_stream_sys_t(p_stream, p_intf, p_sout);
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_stream->pf_add     = Add;
    p_stream->pf_del     = Del;
    p_stream->pf_send    = Send;
    p_stream->pf_flush   = Flush;
    p_stream->pf_control = Control;

    p_stream->p_sys = p_sys;
    free(psz_var_mux);
    free(psz_var_mime);
    return VLC_SUCCESS;

error:
    delete p_sys;
    sout_StreamChainDelete(p_sout, p_sout);
    free(psz_var_mux);
    free(psz_var_mime);
    return VLC_EGENERIC;
}

void SoutClose(vlc_object_t *p_this)
{
    sout_stream_t *p_sout = reinterpret_cast<sout_stream_t*>(p_this);
    delete p_sout->p_sys;
}

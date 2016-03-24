/*****************************************************************************
 * chromecast.cpp: Chromecast demux filter module for vlc
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

#include <vlc_demux.h>
#include <vlc_input.h>

#include <cassert>

static int DemuxOpen(vlc_object_t *);
static void DemuxClose(vlc_object_t *);

vlc_module_begin ()
    set_shortname( "cc_demux" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_( "chromecast demux wrapper" ) )
    set_capability( "demux", 0 )
    add_shortcut( "cc_demux" )
    set_callbacks( DemuxOpen, DemuxClose )
vlc_module_end ()

struct demux_sys_t
{
    demux_sys_t(demux_t * const demux, vlc_renderer * const renderer)
        :p_demux(demux)
        ,p_renderer(renderer)
        ,i_length(-1)
        ,demuxReady(false)
    {
    }

    ~demux_sys_t()
    {
        vlc_object_release( p_renderer );
    }

    mtime_t getPlaybackTime() {
        vlc_mutex_locker locker(&p_renderer->p_sys->lock);
        return p_renderer->p_sys->getPlaybackTime();
    }

    double getPlaybackPosition() {
        vlc_mutex_locker locker(&p_renderer->p_sys->lock);
        return p_renderer->p_sys->getPlaybackPosition(i_length);
    }

    void setLength(mtime_t length) {
        this->i_length = length;
    }

    int Demux() {
        vlc_mutex_lock(&p_renderer->p_sys->lock);
        if (!demuxReady)
        {
            msg_Dbg(p_demux, "wait to demux");
            mutex_cleanup_push(&p_renderer->p_sys->lock);
            while (p_renderer->p_sys->getConnectionStatus() != CHROMECAST_APP_STARTED &&
                   p_renderer->p_sys->getConnectionStatus() != CHROMECAST_CONNECTION_DEAD)
                vlc_cond_wait(&p_renderer->p_sys->loadCommandCond, &p_renderer->p_sys->lock);
            vlc_cleanup_pop();

            demuxReady = true;
            msg_Dbg(p_demux, "ready to demux");
        }

        if (p_renderer->p_sys->getConnectionStatus() != CHROMECAST_APP_STARTED) {
            msg_Dbg(p_demux, "app not started:%d, don't demux", p_renderer->p_sys->getConnectionStatus());
            vlc_mutex_unlock(&p_renderer->p_sys->lock);
            return 0;
        }

        vlc_mutex_unlock(&p_renderer->p_sys->lock);

        return p_demux->p_source->pf_demux( p_demux->p_source );
    }

protected:
    demux_t       * const p_demux;
    vlc_renderer  * const p_renderer;
    mtime_t       i_length;
    bool          demuxReady;

private:
    int source_Control(int cmd, ...)
    {
        va_list ap;
        int ret;

        va_start(ap, cmd);
        ret = p_demux->p_source->pf_control(p_demux->p_source, cmd, ap);
        va_end(ap);
        return ret;
    }
};

static int DemuxDemux( demux_t *p_demux )
{
    return p_demux->p_sys->Demux();
}

static int DemuxControl( demux_t *p_demux, int i_query, va_list args)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    switch (i_query)
    {
    case DEMUX_GET_POSITION:
        *va_arg( args, double * ) = p_sys->getPlaybackPosition();
        return VLC_SUCCESS;

    case DEMUX_GET_TIME:
        *va_arg(args, int64_t *) = p_sys->getPlaybackTime();
        return VLC_SUCCESS;

    case DEMUX_GET_LENGTH:
    {
        int ret;
        va_list ap;

        va_copy( ap, args );
        ret = p_demux->p_source->pf_control( p_demux->p_source, i_query, args );
        if( ret == VLC_SUCCESS )
            p_sys->setLength( *va_arg( ap, int64_t * ) );
        va_end( ap );
        return ret;
    }

    }

    return p_demux->p_source->pf_control( p_demux->p_source, i_query, args );
}

int DemuxOpen(vlc_object_t *p_this)
{
    demux_t *p_demux = reinterpret_cast<demux_t*>(p_this);
    if ( unlikely( p_demux->p_source == NULL ) )
        return VLC_EBADVAR;

    vlc_renderer *p_renderer = input_HoldRenderer( p_demux->p_input );
    if ( p_renderer == NULL ) {
        msg_Err(p_demux, "Missing the control interface to work");
        return VLC_EBADVAR;
    }

    demux_sys_t *p_sys = new(std::nothrow) demux_sys_t(p_demux, p_renderer);
    if (unlikely(p_sys == NULL))
    {
        vlc_object_release( p_renderer );
        return VLC_ENOMEM;
    }

    p_demux->pf_demux = DemuxDemux;
    p_demux->pf_control = DemuxControl;

    vlc_object_hold(p_demux->p_source);

    p_demux->p_sys = p_sys;
    return VLC_SUCCESS;
}

void DemuxClose(vlc_object_t *p_this)
{
    demux_t *p_demux = reinterpret_cast<demux_t*>(p_this);
    demux_sys_t *p_sys = p_demux->p_sys;

    demux_Delete(p_demux->p_source);
    p_demux->s = NULL; /* only the one demux can have a stream source */

    delete p_sys;
}

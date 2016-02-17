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
    demux_sys_t(demux_t *demux, vlc_renderer *intf)
        :p_demux(demux)
        ,p_intf(intf)
        ,i_length(-1)
        ,demuxReady(false)
        ,canSeek(false)
    {
        assert(p_intf != NULL);
        vlc_object_hold(p_intf);
    }

    ~demux_sys_t()
    {
        vlc_object_release(p_intf);
    }

    mtime_t getPlaybackTime() {
        vlc_mutex_locker locker(&p_intf->p_sys->lock);
        return p_intf->p_sys->getPlaybackTime();
    }

    double getPlaybackPosition() {
        vlc_mutex_locker locker(&p_intf->p_sys->lock);
        return p_intf->p_sys->getPlaybackPosition(i_length);
    }

    void setCanSeek(bool canSeek) {
        this->canSeek = canSeek;
    }

    bool seekTo(double pos) {
        if (i_length == -1)
            return false;
        return seekTo( mtime_t( i_length * pos ) );
    }

    bool seekTo(mtime_t i_pos) {
        if (!canSeek)
            return false;
        return p_intf->p_sys->seekTo(i_pos);
    }

    void setLength(mtime_t length) {
        this->i_length = length;
    }

    int Demux() {
        vlc_mutex_lock(&p_intf->p_sys->lock);
        if (!demuxReady)
        {
            msg_Dbg(p_demux, "wait to demux");
            mutex_cleanup_push(&p_intf->p_sys->lock);
            while (p_intf->p_sys->getConnectionStatus() != CHROMECAST_APP_STARTED &&
                   p_intf->p_sys->getConnectionStatus() != CHROMECAST_CONNECTION_DEAD)
                vlc_cond_wait(&p_intf->p_sys->loadCommandCond, &p_intf->p_sys->lock);
            vlc_cleanup_pop();

            demuxReady = true;
            msg_Dbg(p_demux, "ready to demux");
        }

        if (p_intf->p_sys->getConnectionStatus() != CHROMECAST_APP_STARTED) {
            msg_Dbg(p_demux, "app not started:%d, don't demux", p_intf->p_sys->getConnectionStatus());
            vlc_mutex_unlock(&p_intf->p_sys->lock);
            return 0;
        }

        /* hold the data while seeking */
        /* wait until the client is buffering for seeked data */
        if (p_intf->p_sys->i_seektime != -1)
        {
            const mtime_t i_seek_time = p_intf->p_sys->m_seektime; // - (p_intf->p_sys->playback_start_chromecast - p_intf->p_sys->playback_start_local);
            msg_Dbg(p_demux, "%ld do the actual seek", GetCurrentThreadId());
            int i_ret = source_Control( DEMUX_SET_TIME, i_seek_time );
            if (i_ret != VLC_SUCCESS)
            {
                msg_Warn(p_demux, "failed to seek in the muxer %d", i_ret);
                vlc_mutex_unlock(&p_intf->p_sys->lock);
                return 0;
            }

            mutex_cleanup_push(&p_intf->p_sys->lock);
            while (p_intf->p_sys->playback_start_chromecast < p_intf->p_sys->i_seektime)
            {
#ifndef NDEBUG
                msg_Dbg(p_demux, "%ld waiting for Chromecast seek", GetCurrentThreadId());
#endif
                vlc_cond_wait(&p_intf->p_sys->seekCommandCond, &p_intf->p_sys->lock);
#ifndef NDEBUG
                msg_Dbg(p_demux, "%ld finished waiting for Chromecast seek", GetCurrentThreadId());
#endif
            }
            vlc_cleanup_pop();

            p_intf->p_sys->m_seektime = -1;
            p_intf->p_sys->i_seektime = -1;

            if (p_intf->p_sys->getConnectionStatus() != CHROMECAST_APP_STARTED) {
                msg_Warn(p_demux, "cannot seek as the Chromecast app is not running %d", p_intf->p_sys->getConnectionStatus());
                vlc_mutex_unlock(&p_intf->p_sys->lock);
                return 0;
            }
        }
        vlc_mutex_unlock(&p_intf->p_sys->lock);

        return p_demux->p_source->pf_demux( p_demux->p_source );
    }

    bool seekViaChromecast() {
        return !p_intf->p_sys->forceSeekPosition() && getPlaybackTime() != -1;
    }

    void resetForcedSeek() {
        p_intf->p_sys->resetForcedSeek( i_length );
    }

protected:
    demux_t       *p_demux;
    vlc_renderer  *p_intf;
    mtime_t       i_length;
    bool          demuxReady;
    bool          canSeek;

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

    case DEMUX_CAN_SEEK:
    {
        int ret;
        va_list ap;

        va_copy( ap, args );
        ret = p_demux->p_source->pf_control( p_demux->p_source, i_query, args );
        if( ret == VLC_SUCCESS )
            p_sys->setCanSeek( *va_arg( ap, bool* ) );
        va_end( ap );
        return ret;
    }

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

    case DEMUX_SET_POSITION:
    {
        int ret = VLC_SUCCESS;
        va_list ap;

        va_copy( ap, args );
        double pos = va_arg( ap, double );
        va_end( ap );

        if (!p_sys->seekViaChromecast())
        {
            msg_Dbg( p_demux, "internal seek to %f when the playback didn't start", pos );
            p_sys->resetForcedSeek( );
            break; // seek before started, likely on-the-fly restart
        }

        if (!p_sys->seekTo( pos ))
        {
            msg_Err( p_demux, "failed to seek to %f", pos );
            ret = VLC_EGENERIC;
        }
        return ret;
    }

    case DEMUX_SET_TIME:
    {
        int ret = VLC_SUCCESS;
        va_list ap;

        va_copy( ap, args );
        mtime_t pos = va_arg( ap, mtime_t );
        va_end( ap );

        if (!p_sys->seekViaChromecast())
        {
            msg_Dbg( p_demux, "internal seek to %" PRId64 " when the playback didn't start", pos );
            p_sys->resetForcedSeek( );
            break; // seek before started, likely on-the-fly restart
        }

        if (!p_sys->seekTo( pos ))
        {
            msg_Err( p_demux, "failed to seek to time %" PRId64, pos );
            return VLC_EGENERIC;
        }
        return ret;
    }

#if 0
    case DEMUX_GET_PTS_DELAY:
    {
        int ret;
        va_list ap;

        va_copy( ap, args );
        ret = p_demux->p_source->pf_control( p_demux->p_source, i_query, args );
        //if( ret == VLC_SUCCESS )
        //    p_sys->setCanSeek( *va_arg( ap, bool* ) );
        va_end( ap );
        return ret;
    }
#endif
    }

    return p_demux->p_source->pf_control( p_demux->p_source, i_query, args );
}

int DemuxOpen(vlc_object_t *p_this)
{
    demux_t *p_demux = reinterpret_cast<demux_t*>(p_this);
    if (p_demux->p_source == NULL)
        return VLC_EBADVAR;

    vlc_renderer *p_intf = static_cast<vlc_renderer*>(var_InheritAddress(p_demux, SOUT_INTF_ADDRESS));
    if (p_intf == NULL) {
        msg_Err(p_demux, "Missing the control interface to work");
        return VLC_EBADVAR;
    }

    demux_sys_t *p_sys = new(std::nothrow) demux_sys_t(p_demux, p_intf);
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

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

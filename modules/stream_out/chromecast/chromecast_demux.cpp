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

#define DEMUXFILTER_CFG_PREFIX "demux-filter-chromecast-"

static const char *const ppsz_sout_options[] = {
    "control", NULL
};

vlc_module_begin ()
    set_shortname( "cc_demux" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_( "chromecast demux wrapper" ) )
    set_capability( "demux", 0 )
    add_shortcut( "cc_demux" )
    set_callbacks( DemuxOpen, DemuxClose )

    add_integer(DEMUXFILTER_CFG_PREFIX "control", NULL, "", "", false)

vlc_module_end ()

struct demux_sys_t
{
    demux_sys_t(demux_t * const demux, vlc_renderer_sys * const renderer)
        :p_demux(demux)
        ,p_renderer(renderer)
        ,i_length(-1)
        ,demuxReady(false)
        ,canSeek(false)
        ,m_seektime( VLC_TS_INVALID )
    {
        input_item_t *p_item = input_GetItem( p_demux->p_input );
        if ( p_item )
        {
            char *psz_title = input_item_GetTitleFbName( p_item );
            p_renderer->setTitle( psz_title );
            free( psz_title );

            psz_title = input_item_GetArtworkURL( p_item );
            p_renderer->setArtwork( psz_title );
            free( psz_title );
        }
    }

    ~demux_sys_t()
    {
        p_renderer->setTitle( NULL );
        p_renderer->setArtwork( NULL );
    }

    /**
     * @brief getPlaybackTime
     * @return the current playback time on the device or VLC_TS_INVALID if unknown
     */
    mtime_t getPlaybackTime()
    {
        vlc_mutex_locker locker(&p_renderer->lock);
        return p_renderer->getPlaybackTimestamp();
    }

    double getPlaybackPosition()
    {
        vlc_mutex_locker locker(&p_renderer->lock);
        return p_renderer->getPlaybackPosition(i_length);
    }

    void setCanSeek( bool canSeek )
    {
        this->canSeek = canSeek;
    }

    bool seekTo( double pos )
    {
        if (i_length == -1)
            return false;
        return seekTo( mtime_t( i_length * pos ) );
    }

    bool seekTo( mtime_t i_pos )
    {
        if ( !canSeek )
            return false;

        if ( p_renderer->seekTo( i_pos ) )
        {
            /* seeking will be handled with the Chromecast */
            m_seektime = i_pos;
            return true;
        }
        return false;
    }

    void setLength( mtime_t length )
    {
        this->i_length = length;
    }

    int Demux() {
        vlc_mutex_lock(&p_renderer->lock);
        if (!demuxReady)
        {
            msg_Dbg(p_demux, "wait to demux");
            p_renderer->waitAppStarted();
            demuxReady = true;
            msg_Dbg(p_demux, "ready to demux");
        }

        if (p_renderer->getConnectionStatus() != CHROMECAST_APP_STARTED)
        {
            msg_Dbg(p_demux, "app not started:%d, don't demux", p_renderer->getConnectionStatus());
            vlc_mutex_unlock(&p_renderer->lock);
            return VLC_DEMUXER_EOF;
        }

        /* hold the data while seeking */
        /* wait until the device is buffering for seeked data */
        if ( m_seektime != VLC_TS_INVALID )
        {
            msg_Dbg( p_demux, "do the actual seek" );
            int i_ret = source_Control( DEMUX_SET_TIME, m_seektime );
            m_seektime = VLC_TS_INVALID;
            if (i_ret != VLC_SUCCESS)
            {
                msg_Warn( p_demux, "failed to seek in the muxer %d", i_ret );
                vlc_mutex_unlock(&p_renderer->lock);
                return VLC_DEMUXER_EGENERIC;
            }

            p_renderer->waitSeekDone();

            if (p_renderer->getConnectionStatus() != CHROMECAST_APP_STARTED) {
                msg_Warn(p_demux, "cannot seek as the Chromecast app is not running %d", p_renderer->getConnectionStatus());
                vlc_mutex_unlock(&p_renderer->lock);
                return VLC_DEMUXER_EOF;
            }
        }
        vlc_mutex_unlock(&p_renderer->lock);

        return demux_Demux( p_demux->p_source );
    }

protected:
    demux_t       * const p_demux;
    vlc_renderer_sys  * const p_renderer;
    mtime_t       i_length;
    bool          demuxReady;
    bool          canSeek;
    /* seek time kept while waiting for the chromecast to "seek" */
    mtime_t       m_seektime;

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

        if ( p_sys->getPlaybackTime() == VLC_TS_INVALID )
        {
            msg_Dbg( p_demux, "internal seek to %f when the playback didn't start", pos );
            break; // seek before device started, likely on-the-fly restart
        }

        if ( !p_sys->seekTo( pos ) )
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

        if ( p_sys->getPlaybackTime() == VLC_TS_INVALID )
        {
            msg_Dbg( p_demux, "internal seek to %" PRId64 " when the playback didn't start", pos );
            break; // seek before device started, likely on-the-fly restart
        }

        if ( !p_sys->seekTo( pos ) )
        {
            msg_Err( p_demux, "failed to seek to time %" PRId64, pos );
            return VLC_EGENERIC;
        }
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

    config_ChainParse(p_demux, DEMUXFILTER_CFG_PREFIX, ppsz_sout_options, p_demux->p_cfg);

    intptr_t i_chromecast_control = var_InheritInteger( p_demux, DEMUXFILTER_CFG_PREFIX "control" );
    if ( i_chromecast_control == 0 )
    {
        msg_Err(p_demux, "Missing the control interface to work");
        return VLC_EBADVAR;
    }
    vlc_renderer_sys *p_renderer = (vlc_renderer_sys *) i_chromecast_control;

    demux_sys_t *p_sys = new(std::nothrow) demux_sys_t(p_demux, p_renderer);
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_demux->pf_demux = DemuxDemux;
    p_demux->pf_control = DemuxControl;

    p_demux->p_sys = p_sys;
    return VLC_SUCCESS;
}

void DemuxClose(vlc_object_t *p_this)
{
    demux_t *p_demux = reinterpret_cast<demux_t*>(p_this);
    demux_sys_t *p_sys = p_demux->p_sys;

    delete p_sys;
}

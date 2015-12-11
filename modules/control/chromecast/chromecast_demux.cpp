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

#include <vlc_common.h>
#include <vlc_plugin.h>
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
    demux_sys_t(demux_t *demux, intf_thread_t *intf)
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

    void setCanPause(bool canPause) {
        p_intf->p_sys->setCanPause( canPause );
    }

    bool seekTo(double pos) {
        if (i_length == -1)
            return false;
        return seekTo( mtime_t( i_length * pos ) );
    }

    bool seekTo(mtime_t i_pos) {
        if (!canSeek)
            return false;
#if 0 /* TODO */
        return p_intf->p_sys->seekTo(i_pos);
#else
        return false;
#endif
    }

    void setLength(mtime_t length) {
        this->i_length = length;
    }

    int Demux() {
        vlc_mutex_lock(&p_intf->p_sys->lock);
        if (!demuxReady)
        {
            mutex_cleanup_push(&p_intf->p_sys->lock);
            while ((p_intf->p_sys->getConnectionStatus() != CHROMECAST_APP_STARTED &&
                   p_intf->p_sys->getConnectionStatus() != CHROMECAST_DEAD) ||
                   !p_intf->p_sys->currentStopped)
                vlc_cond_wait(&p_intf->p_sys->loadCommandCond, &p_intf->p_sys->lock);
            vlc_cleanup_pop();

            demuxReady = true;
            msg_Dbg(p_demux, "ready to demux");
        }

        if (p_intf->p_sys->getConnectionStatus() != CHROMECAST_APP_STARTED) {
            vlc_mutex_unlock(&p_intf->p_sys->lock);
            return 0;
        }

        /* hold the data while seeking */
        /* wait until the client is buffering for seeked data */
        if (p_intf->p_sys->i_seektime != -1.0)
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
                msg_Dbg(p_demux, "%ld waiting for Chromecast seek", GetCurrentThreadId());
                vlc_cond_wait(&p_intf->p_sys->seekCommandCond, &p_intf->p_sys->lock);
                msg_Dbg(p_demux, "%ld finished waiting for Chromecast seek", GetCurrentThreadId());
            }
            vlc_cleanup_pop();

            p_intf->p_sys->m_seektime = -1.0;
            p_intf->p_sys->i_seektime = -1.0;

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
        return !p_intf->p_sys->forceSeekPosition() && getPlaybackTime() != -1.0;
    }

    void resetForcedSeek() {
        p_intf->p_sys->resetForcedSeek( i_length );
    }

protected:
    demux_t       *p_demux;
    intf_thread_t *p_intf;
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

static const char *demux_control_names[] = {
    "DEMUX_CAN_SEEK",

    "DEMUX_can_fastseek",

    /** Checks whether (long) pause then stream resumption is supported.
     * Can fail only if synchronous and <b>not</b> an access-demuxer. The
     * underlying input stream then determines if pause is supported.
     * \bug Failing should not be allowed.
     *
     * arg1= bool * */
    "DEMUX_CAN_PAUSE",

    /** Whether the stream can be read at an arbitrary pace.
     * Cannot fail.
     *
     * arg1= bool * */
    "DEMUX_CAN_CONTROL_PACE",

    "DEMUX_get_size",
    "DEMUX_is_directory",

    /** Retrieves the PTS delay (roughly the default buffer duration).
     * Can fail only if synchronous and <b>not</b> an access-demuxer. The
     * underlying input stream then determines the PTS delay.
     *
     * arg1= int64_t * */
    "DEMUX_GET_PTS_DELAY",

    /**
     * \todo Document
     *
     * \warning The prototype is different from STREAM_GET_TITLE_INFO
     *
     * Can fail", meaning there is only one title and one chapter.
     *
     * arg1= input_title_t ***", arg2=int *", arg3=int *pi_title_offset(0)",
     * arg4= int *pi_seekpoint_offset(0) */
    "DEMUX_GET_TITLE_INFO",

    "DEMUX_get_title",
    "DEMUX_get_seekpoint",

    /** Retrieves stream meta-data.
     * Should fail if no meta-data were retrieved.
     *
     * arg1= vlc_meta_t * */
    "DEMUX_GET_META",

    "DEMUX_get_content_type",

    /** Retrieves an estimate of signal quality and strength.
     * Can fail.
     *
     * arg1=double *quality", arg2=double *strength */
    "DEMUX_GET_SIGNAL",

    /** Sets the paused or playing/resumed state.
     *
     * Streams are initially in playing state. The control always specifies a
     * change from paused to playing (false) or from playing to paused (true)
     * and streams are initially playing; a no-op cannot be requested.
     *
     * The control is never used if "DEMUX_CAN_PAUSE fails.
     * Can fail.
     *
     * arg1= bool */
    "DEMUX_SET_PAUSE_STATE",

    /** Seeks to the beginning of a title.
     *
     * The control is never used if "DEMUX_GET_TITLE_INFO fails.
     * Can fail.
     *
     * arg1= int */
    "DEMUX_SET_TITLE",

    /** Seeks to the beginning of a chapter of the current title.
     *
     * The control is never used if "DEMUX_GET_TITLE_INFO fails.
     * Can fail.
     *
     * arg1= int */
    "DEMUX_SET_SEEKPOINT",        /* arg1= int            can fail */

    /**
     * \todo Document
     *
     * \warning The prototype is different from STREAM_SET_RECORD_STATE
     *
     * The control is never used if "DEMUX_CAN_RECORD fails or returns false.
     * Can fail.
     *
     * arg1= bool */
    "DEMUX_SET_RECORD_STATE",

    /* I. Common queries to access_demux and demux */
    /* POSITION double between 0.0 and 1.0 */
    "DEMUX_GET_POSITION", /* arg1= double *       res=    */
    "DEMUX_SET_POSITION",         /* arg1= double arg2= bool b_precise    res=can fail    */

    /* LENGTH/TIME in microsecond", 0 if unknown */
    "DEMUX_GET_LENGTH",           /* arg1= int64_t *      res=    */
    "DEMUX_GET_TIME",             /* arg1= int64_t *      res=    */
    "DEMUX_SET_TIME",             /* arg1= int64_t arg2= bool b_precise   res=can fail    */

    /* "DEMUX_SET_GROUP/SET_ES only a hint for demuxer (mainly DVB) to allow not
     * reading everything (you should not use this to call es_out_Control)
     * if you don't know what to do with it", just IGNORE it", it is safe(r)
     * -1 means all group", 0 default group (first es added) */
    "DEMUX_SET_GROUP",            /* arg1= int", arg2=const vlc_list_t *   can fail */
    "DEMUX_SET_ES",               /* arg1= int                            can fail */

    /* Ask the demux to demux until the given date at the next pf_demux call
     * but not more (and not less", at the precision available of course).
     * XXX: not mandatory (except for subtitle demux) but will help a lot
     * for multi-input
     */
    "DEMUX_SET_NEXT_DEMUX_TIME",  /* arg1= int64_t        can fail */
    /* FPS for correct subtitles handling */
    "DEMUX_GET_FPS",              /* arg1= double *       res=can fail    */

    /* Meta data */
    "DEMUX_HAS_UNSUPPORTED_META", /* arg1= bool *   res can fail    */

    /* Attachments */
    "DEMUX_GET_ATTACHMENTS",      /* arg1=input_attachment_t***", int* res=can fail */

    /* RECORD you are ensured that it is never called twice with the same state
     * you should accept it only if the stream can be recorded without
     * any modification or header addition. */
    "DEMUX_CAN_RECORD",           /* arg1=bool*   res=can fail(assume false) */

    /* II. Specific access_demux queries */

    /* "DEMUX_CAN_CONTROL_RATE is called only if "DEMUX_CAN_CONTROL_PACE has returned false.
     * *pb_rate should be true when the rate can be changed (using "DEMUX_SET_RATE)
     * *pb_ts_rescale should be true when the timestamps (pts/dts/pcr) have to be rescaled */
    "DEMUX_CAN_CONTROL_RATE",     /* arg1= bool*pb_rate arg2= bool*pb_ts_rescale  can fail(assume false) */
    /* "DEMUX_SET_RATE is called only if "DEMUX_CAN_CONTROL_RATE has returned true.
     * It should return the value really used in *pi_rate */
    "DEMUX_SET_RATE",             /* arg1= int*pi_rate                                        can fail */

    /** Checks whether the stream is actually a playlist", rather than a real
     * stream.
     *
     * \warning The prototype is different from STREAM_IS_DIRECTORY.
     *
     * Can fail if the stream is not a playlist (same as returning false).
     *
     * arg1= bool * */
    "DEMUX_IS_PLAYLIST",

    /* Navigation */
    "DEMUX_NAV_ACTIVATE",        /* res=can fail */
    "DEMUX_NAV_UP",              /* res=can fail */
    "DEMUX_NAV_DOWN",            /* res=can fail */
    "DEMUX_NAV_LEFT",            /* res=can fail */
    "DEMUX_NAV_RIGHT",           /* res=can fail */
};

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

    case DEMUX_CAN_PAUSE:
    {
        int ret;
        va_list ap;

        va_copy( ap, args );
        ret = p_demux->p_source->pf_control( p_demux->p_source, i_query, args );
        if( ret == VLC_SUCCESS )
            p_sys->setCanPause( *va_arg( ap, bool* ) );
        va_end( ap );
        return ret;
    }

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

    msg_Dbg(p_demux, "DemuxControl %s", demux_control_names[i_query]);

    return p_demux->p_source->pf_control( p_demux->p_source, i_query, args );
}

int DemuxOpen(vlc_object_t *p_this)
{
    demux_t *p_demux = reinterpret_cast<demux_t*>(p_this);
    if (p_demux->p_source == NULL)
        return VLC_EBADVAR;

    intf_thread_t *p_intf = static_cast<intf_thread_t*>(var_InheritAddress(p_demux, SOUT_INTF_ADDRESS));
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

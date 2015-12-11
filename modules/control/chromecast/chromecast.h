/*****************************************************************************
 * chromecast.cpp: Chromecast module for vlc
 *****************************************************************************
 * Copyright © 2014-2015 VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Steve Lhomme <robux4@videolabs.io>
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

#ifndef VLC_CHROMECAST_H
#define VLC_CHROMECAST_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_POLL
# include <poll.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_network.h>
#include <vlc_tls.h>
#include <vlc_interrupt.h>
#include <vlc_demux.h>
#include <vlc_sout.h>
#include <vlc_access.h>

#include <atomic>
#include <cassert>
#include <sstream>
#include <queue>

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include "cast_channel.pb.h"

#include "../../misc/webservices/json.h"

// Status
enum connection_status {
    CHROMECAST_DISCONNECTED,
    CHROMECAST_TLS_CONNECTED,
    CHROMECAST_AUTHENTICATED,
    CHROMECAST_APP_STARTED,
    CHROMECAST_DEAD,
};

enum command_status {
    NO_CMD_PENDING,
    CMD_LOAD_SENT,
    CMD_PLAYBACK_SENT,
    CMD_SEEK_SENT,
};

enum receiver_state {
    RECEIVER_IDLE,
    RECEIVER_PLAYING,
    RECEIVER_BUFFERING,
    RECEIVER_PAUSED,
};

enum receiver_display {
    DISPLAY_UNKNOWN,
    HAS_VIDEO,
    AUDIO_ONLY
};

#define PACKET_MAX_LEN 10 * 1024
#define PACKET_HEADER_LEN 4

// Media player Chromecast app id
static const std::string DEFAULT_CHOMECAST_RECEIVER = "receiver-0";

/* deadline regarding pings sent from receiver */
#define PING_WAIT_TIME 6000
#define PING_WAIT_RETRIES 0
/* deadline regarding pong we expect after pinging the receiver */
#define PONG_WAIT_TIME 500
#define PONG_WAIT_RETRIES 2

#define TIMEOUT_LOAD_CMD  (6 * CLOCK_FREQ)
#define TIMEOUT_MDNS_IP   (4 * CLOCK_FREQ)

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    intf_sys_t(intf_thread_t *intf)
        :p_intf(intf)
        ,p_input(NULL)
        ,devicePort(8009)
        ,receiverState(RECEIVER_IDLE)
        ,date_play_start(-1)
        ,playback_start_chromecast(-1.0)
        ,playback_start_local(0)
        ,canPause(false)
        ,canDisplay(DISPLAY_UNKNOWN)
        ,currentStopped(true)
        ,i_sock_fd(-1)
        ,p_creds(NULL)
        ,p_tls(NULL)
        ,cmd_status(NO_CMD_PENDING)
        ,i_supportedMediaCommands(15)
        ,m_seektime(-1.0)
        ,i_seektime(-1.0)
        ,conn_status(CHROMECAST_DISCONNECTED)
        ,i_app_requestId(0)
        ,i_requestId(0)
        ,i_sout_id(0)
        ,b_restart_playback(false)
        ,b_forcing_position(false)
    {
        //p_interrupt = vlc_interrupt_create();
    }

    ~intf_sys_t()
    {
        ipChangedEvent( NULL );

        //vlc_interrupt_destroy(p_interrupt);
    }

    mtime_t getPlaybackTime() const {
        switch( receiverState )
        {
        case RECEIVER_PLAYING:
            return ( mdate() - date_play_start ) + playback_start_local;

        case RECEIVER_IDLE:
            msg_Dbg(p_intf, "receiver idle using buffering time %" PRId64, playback_start_local);
            break;
        case RECEIVER_BUFFERING:
            msg_Dbg(p_intf, "receiver buffering using buffering time %" PRId64, playback_start_local);
            break;
        case RECEIVER_PAUSED:
            msg_Dbg(p_intf, "receiver paused using buffering time %" PRId64, playback_start_local);
            break;
        }
        return playback_start_local;
    }

    double getPlaybackPosition(mtime_t i_length) const {
        if( i_length > 0 && date_play_start > 0)
            return (double) getPlaybackTime() / (double)( i_length );
        return 0.0;
    }

    void setCanPause(bool canPause) {
        vlc_mutex_locker locker(&lock);
        this->canPause = canPause;
    }

    void setCanDisplay(receiver_display canDisplay) {
        vlc_mutex_locker locker(&lock);
        this->canDisplay = canDisplay;
        vlc_cond_broadcast(&loadCommandCond);
    }

    bool isFinishedPlaying() {
        vlc_mutex_locker locker(&lock);
        return deviceIP.empty() || conn_status == CHROMECAST_DEAD || (receiverState == RECEIVER_BUFFERING && cmd_status != CMD_SEEK_SENT);
    }

    bool seekTo(mtime_t pos);

    void sendPlayerCmd();

    bool forceSeekPosition() const {
        return b_forcing_position;
    }

    void resetForcedSeek(mtime_t i_length) {
        b_forcing_position = false;
        playback_start_local = i_length * f_restart_position;
    }

    intf_thread_t  * const p_intf;
    input_thread_t *p_input;
    uint16_t       devicePort;
    std::string    deviceIP;
    std::string    serverIP;
    std::string    mime;
    std::string    muxer;

    std::string appTransportId;
    std::string mediaSessionId;
    receiver_state receiverState;

    mtime_t     date_play_start;
    mtime_t     playback_start_chromecast;
    mtime_t     playback_start_local;
    bool        canPause;
    receiver_display  canDisplay;
    bool              currentStopped;

    int i_sock_fd;
    vlc_tls_creds_t *p_creds;
    vlc_tls_t *p_tls;

    enum command_status    cmd_status;
    int                    i_supportedMediaCommands;
    /* internal seek time */
    mtime_t                m_seektime;
    /* seek time with Chromecast relative timestamp */
    mtime_t                i_seektime;

    //vlc_interrupt_t *p_interrupt;
    vlc_mutex_t  lock;
    vlc_cond_t   loadCommandCond;
    vlc_cond_t   seekCommandCond;
    vlc_thread_t chromecastThread;

    void msgAuth();

    void msgReceiverClose();

    void handleMessages();

    void InputUpdated( input_thread_t * );

    connection_status getConnectionStatus() const
    {
        return conn_status;
    }

    void setConnectionStatus(connection_status status)
    {
        if (conn_status != status)
        {
            msg_Dbg(p_intf, "change Chromecast connection status from %d to %d", conn_status, status);
            conn_status = status;
            vlc_cond_broadcast(&loadCommandCond);
        }
    }

    void ipChangedEvent(const char *psz_new_ip);

private:
    int sendMessage(const castchannel::CastMessage &msg);

    void pushMessage(const std::string & namespace_,
                      const std::string & payload,
                      const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER,
                      castchannel::CastMessage_PayloadType payloadType = castchannel::CastMessage_PayloadType_STRING);

    void pushMediaPlayerMessage(const std::stringstream & payload);

    void setPlayerStatus(enum command_status status) {
        if (cmd_status != status)
        {
            msg_Dbg(p_intf, "change Chromecast command status from %d to %d", cmd_status, status);
            cmd_status = status;
            vlc_cond_broadcast(&loadCommandCond);
        }
    }

    enum connection_status conn_status;

    unsigned i_app_requestId;
    unsigned i_requestId;
    unsigned i_sout_id;

    std::string       s_sout;
    std::string       s_chromecast_url;
    bool              canRemux;
    bool              canDoDirect;
    bool              b_restart_playback;
    bool              b_forcing_position;
    double            f_restart_position;

    void msgPing();
    void msgPong();
    void msgConnect(const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER);

    void msgReceiverLaunchApp();
    void msgReceiverGetStatus();
    void msgPlayerGetStatus();

    void msgPlayerLoad();
    void msgPlayerStop();
    void msgPlayerPlay();
    void msgPlayerPause();
    void msgPlayerSeek(const std::string & currentTime);

    std::string GetMedia();

    bool canDecodeVideo( const es_format_t * ) const;
    bool canDecodeAudio( const es_format_t * ) const;

    void processMessage(const castchannel::CastMessage &msg);

    void plugOutputRedirection();
    void unplugOutputRedirection();
    void initiateRestart();
    void disconnectChromecast();
};

#endif /* VLC_CHROMECAST_H */

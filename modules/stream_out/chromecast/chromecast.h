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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_tls.h>
#include <vlc_interrupt.h>

#include <sstream>

#include "cast_channel.pb.h"

#define PACKET_HEADER_LEN 4
#define SOUT_INTF_ADDRESS  "sout-chromecast-intf"

// Media player Chromecast app id
static const std::string DEFAULT_CHOMECAST_RECEIVER = "receiver-0";
/* see https://developers.google.com/cast/docs/reference/messages */
static const std::string NAMESPACE_MEDIA            = "urn:x-cast:com.google.cast.media";


// Status
enum connection_status
{
    CHROMECAST_DISCONNECTED,
    CHROMECAST_TLS_CONNECTED,
    CHROMECAST_AUTHENTICATED,
    CHROMECAST_APP_STARTED,
    CHROMECAST_CONNECTION_DEAD,
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

enum restart_state {
    RESTART_NONE,
    RESTART_STOPPING,
    RESTART_STARTING,
};

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    intf_sys_t(intf_thread_t *intf);
    ~intf_sys_t();

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
        if( i_length > 0 && date_play_start != -1)
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

    bool isFinishedPlaying();

    bool seekTo(mtime_t pos);

    void sendPlayerCmd();

    bool forceSeekPosition() const {
        return b_forcing_position;
    }

    void resetForcedSeek(mtime_t i_length) {
        b_forcing_position = false;
        playback_start_local = i_length * f_restart_position;
        msg_Dbg(p_intf, "resetForcedSeek playback_start_local:%" PRId64, playback_start_local);
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

    /* local date when playback started/resumed */
    mtime_t     date_play_start;
    /* playback time reported by the receiver, used to wait for seeking point */
    mtime_t     playback_start_chromecast;
    /* local playback time of the input when playback started/resumed */
    mtime_t     playback_start_local;
    bool        canPause;
    receiver_display  canDisplay;
    restart_state     restartState;
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

    vlc_mutex_t  lock;
    vlc_cond_t   loadCommandCond;
    vlc_cond_t   seekCommandCond;
    vlc_thread_t chromecastThread;
    vlc_interrupt_t *p_interrupt;

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
#ifndef NDEBUG
            msg_Dbg(p_intf, "change Chromecast connection status from %d to %d", conn_status, status);
#endif
            conn_status = status;
            vlc_cond_broadcast(&loadCommandCond);
        }
    }

    void ipChangedEvent(const char *psz_new_ip);
    void stateChangedForRestart( input_thread_t * );

    void msgPlayerSetVolume(float volume);
    void msgPlayerSetMute(bool mute);

    int connectChromecast();

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

    void msgPing();
    void msgPong();
    void msgConnect(const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER);

    void msgReceiverLaunchApp();
    void msgReceiverGetStatus();

    void msgPlayerLoad();
    void msgPlayerStop();
    void msgPlayerPlay();
    void msgPlayerPause();
    void msgPlayerSeek(const std::string & currentTime);
    void msgPlayerGetStatus();

    enum connection_status conn_status;

    unsigned i_receiver_requestId;
    unsigned i_requestId;
    unsigned i_sout_id;

    std::string       s_sout;
    std::string       s_chromecast_url;
    bool              canRemux;
    bool              canDoDirect;
    bool              b_restart_playback;
    bool              b_forcing_position;
    double            f_restart_position;

    std::string GetMedia();

    bool canDecodeVideo( const es_format_t * ) const;
    bool canDecodeAudio( const es_format_t * ) const;

    void processMessage(const castchannel::CastMessage &msg);

    void plugOutputRedirection();
    void unplugOutputRedirection();
    void disconnectChromecast();
    void setCurrentStopped(bool);

    void restartDoStop();
    bool restartDoPlay();
};

#endif /* VLC_CHROMECAST_H */

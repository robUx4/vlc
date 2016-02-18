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
#include <vlc_renderer.h>
#include <vlc_tls.h>

#include <sstream>

#include "cast_channel.pb.h"

#define PACKET_HEADER_LEN 4
#define SOUT_INTF_ADDRESS  "sout-chromecast-intf"

// Media player Chromecast app id
static const std::string DEFAULT_CHOMECAST_RECEIVER = "receiver-0";
/* see https://developers.google.com/cast/docs/reference/messages */
static const std::string NAMESPACE_MEDIA            = "urn:x-cast:com.google.cast.media";

#define HTTP_PORT               8010  /* TODO move */

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

enum receiver_display {
    DISPLAY_UNKNOWN,
    HAS_VIDEO,
    AUDIO_ONLY
};

enum receiver_state {
    RECEIVER_IDLE,
    RECEIVER_PLAYING,
    RECEIVER_BUFFERING,
    RECEIVER_PAUSED,
};

enum restart_state {
    RESTART_NONE,
    RESTART_STOPPING,
    RESTART_STARTING,
};

/*****************************************************************************
 * vlc_renderer_sys: description and status of interface
 *****************************************************************************/
struct vlc_renderer_sys
{
    vlc_renderer_sys(vlc_renderer * const intf);
    ~vlc_renderer_sys();

    bool isFinishedPlaying() {
        vlc_mutex_locker locker(&lock);
        return deviceIP.empty() || conn_status == CHROMECAST_CONNECTION_DEAD || (receiverState == RECEIVER_BUFFERING && cmd_status != CMD_SEEK_SENT);
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
        if( i_length > 0 && date_play_start != -1)
            return (double) getPlaybackTime() / (double)( i_length );
        return 0.0;
    }

    bool seekTo(mtime_t pos);

    bool forceSeekPosition() const {
        return b_forcing_position;
    }

    void resetForcedSeek(mtime_t i_length) {
        b_forcing_position = false;
        playback_start_local = i_length * f_restart_position;
#ifndef NDEBUG
        msg_Dbg(p_intf, "resetForcedSeek playback_start_local:%" PRId64, playback_start_local);
#endif
    }

    vlc_renderer  * const p_intf;
    input_thread_t *p_input;
    uint16_t       devicePort; /* TODO remove */
    std::string    deviceIP;
    std::string    serverIP;
    std::string    mime;
    std::string    muxer;

    std::string appTransportId;
    std::string mediaSessionId;
    receiver_state receiverState;

    receiver_display  canDisplay;
    bool              currentStopped;

    int i_sock_fd;
    vlc_tls_creds_t *p_creds;
    vlc_tls_t *p_tls;

    /* local date when playback started/resumed */
    mtime_t           date_play_start;
    /* playback time reported by the receiver, used to wait for seeking point */
    mtime_t           playback_start_chromecast;
    /* local playback time of the input when playback started/resumed */
    mtime_t           playback_start_local;
    /* internal seek time */
    mtime_t           m_seektime;
    /* seek time with Chromecast relative timestamp */
    mtime_t           i_seektime;
    restart_state     restartState; /* TODO remove this */


    vlc_mutex_t  lock;
    vlc_cond_t   loadCommandCond;
    vlc_cond_t   seekCommandCond;
    vlc_thread_t chromecastThread;

    void msgAuth();
    void msgReceiverClose(std::string destinationId);

    void handleMessages();
    void sendPlayerCmd();

    void InputUpdated( input_thread_t * );

    void setCanDisplay(receiver_display canDisplay) {
        vlc_mutex_locker locker(&lock);
        this->canDisplay = canDisplay;
        vlc_cond_broadcast(&loadCommandCond);
    }

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

    void deviceChanged(const vlc_renderer_item *p_item);
    int connectChromecast();
    void disconnectChromecast();

    float msgPlayerGetVolume();
    void msgPlayerSetVolume(float volume);
    bool msgPlayerGetMute();
    void msgPlayerSetMute(bool mute);

private:
    void msgPing();
    void msgPong();
    void msgConnect(const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER);

    void msgReceiverLaunchApp();
    void msgReceiverGetStatus();

    void msgPlayerLoad();
    void msgPlayerStop();
    void msgPlayerPlay();
    void msgPlayerPause();
    void msgPlayerGetStatus();
    void msgPlayerSeek(const std::string & currentTime);

    void processMessage(const castchannel::CastMessage &msg);

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
        }
    }

    enum connection_status conn_status;
    enum command_status    cmd_status;

    unsigned i_receiver_requestId;
    unsigned i_requestId;
    unsigned i_sout_id;

    std::string       s_sout;
    std::string       s_chromecast_url;
    bool              canRemux;    ///< can be played by just remuxing
    bool              canDoDirect; ///< can use the URL directly
    bool              b_forcing_position;
    double            f_restart_position;

    float             f_volume;
    bool              b_muted;

    std::string GetMedia();

    bool canDecodeVideo( const es_format_t * ) const;
    bool canDecodeAudio( const es_format_t * ) const;

    void plugOutputRedirection();
    void unplugOutputRedirection();
    void setCurrentStopped(bool);
};

#endif /* VLC_CHROMECAST_H */

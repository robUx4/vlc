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
#include <vlc_interface.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_tls.h>

#include <queue>
#include <sstream>

#include "cast_channel.pb.h"

#define PACKET_HEADER_LEN 4

// Media player Chromecast app id
static const std::string DEFAULT_CHOMECAST_RECEIVER = "receiver-0";
/* see https://developers.google.com/cast/docs/reference/messages */
static const std::string NAMESPACE_MEDIA            = "urn:x-cast:com.google.cast.media";

#define HTTP_PORT               8010

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

struct intf_sys_t
{
    intf_sys_t(intf_thread_t * const intf);
    ~intf_sys_t();


    intf_thread_t  * const p_stream;
    input_thread_t *p_input;
    std::string    serverIP;
    std::string    mime;
    std::string    muxer;

    std::string appTransportId;
    std::string mediaSessionId;

    int i_sock_fd;
    vlc_tls_creds_t *p_creds;
    vlc_tls_t *p_tls;

    vlc_mutex_t  lock;
    vlc_cond_t   loadCommandCond;
    vlc_thread_t chromecastThread;

    void msgAuth();
    void msgReceiverClose(std::string destinationId);

    void handleMessages();
    int sendMessages();
    void sendPlayerCmd();

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
            msg_Dbg(p_stream, "change Chromecast connection status from %d to %d", conn_status, status);
#endif
            conn_status = status;
            vlc_cond_broadcast(&loadCommandCond);
        }
    }

    int connectChromecast(char *psz_ipChromecast);
    void disconnectChromecast();

    void msgPlayerSetVolume(float volume);
    void msgPlayerSetMute(bool mute);

    void msgPing();
    void msgPong();
    void msgConnect(const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER);

    void msgReceiverLaunchApp();
    void msgReceiverGetStatus();

    void msgPlayerLoad();
    void msgPlayerPlay();
    void msgPlayerPause();

    std::queue<castchannel::CastMessage> messagesToSend;

    void processMessage(const castchannel::CastMessage &msg);

private:
    int sendMessage(castchannel::CastMessage &msg);

    void buildMessage(const std::string & namespace_,
                      const std::string & payload,
                      const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER,
                      castchannel::CastMessage_PayloadType payloadType = castchannel::CastMessage_PayloadType_STRING);

    void setPlayerStatus(enum command_status status) {
        if (cmd_status != status)
        {
            msg_Dbg(p_stream, "change Chromecast command status from %d to %d", cmd_status, status);
            cmd_status = status;
            vlc_cond_broadcast(&loadCommandCond);
        }
    }

    enum connection_status conn_status;
    enum command_status    cmd_status;

    unsigned i_receiver_requestId;
    unsigned i_requestId;
    unsigned i_sout_id;

    std::string       s_sout;
    std::string       s_chromecast_url;
    std::string GetMedia();

    void plugOutputRedirection();
    void unplugOutputRedirection();
};

#endif /* VLC_CHROMECAST_H */

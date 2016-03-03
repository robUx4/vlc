/*****************************************************************************
 * chromecast.cpp: Chromecast common code between modules for vlc
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "chromecast.h"

#include <cassert>

static const mtime_t SEEK_FORWARD_OFFSET = 1000000;

bool vlc_renderer_sys::seekTo(mtime_t pos)
{
    vlc_mutex_locker locker(&lock);
    if (conn_status == CHROMECAST_CONNECTION_DEAD)
        return false;

    assert(playback_start_chromecast != -1);

    char current_time[32];
    i_seektime = mdate() + SEEK_FORWARD_OFFSET /* + playback_start_local */ ;
    if( snprintf( current_time, sizeof(current_time), "%f", double( i_seektime ) / 1000000.0 ) >= (int)sizeof(current_time) )
    {
        msg_Err( p_intf, "snprintf() truncated string for mediaSessionId" );
        current_time[sizeof(current_time) - 1] = '\0';
    }
    m_seektime = pos; // + playback_start_chromecast - playback_start_local;
    playback_start_local = pos;
#ifndef NDEBUG
    msg_Dbg( p_intf, "%ld Seeking to %" PRId64 "/%s playback_time:%" PRId64 " playback_start_local:%" PRId64, GetCurrentThreadId(), pos, current_time, playback_start_chromecast, playback_start_local);
#endif
    setPlayerStatus(CMD_SEEK_SENT);
    msgPlayerSeek( current_time );

    return true;
}

/**
 * @brief Send a message to the Chromecast
 * @param msg the CastMessage to send
 * @return vlc error code
 */
int vlc_renderer_sys::sendMessage(const castchannel::CastMessage &msg)
{
    int i_size = msg.ByteSize();
    uint8_t *p_data = new(std::nothrow) uint8_t[PACKET_HEADER_LEN + i_size];
    if (p_data == NULL)
        return VLC_ENOMEM;

#ifndef NDEBUG
    msg_Dbg(p_intf, "sendMessage: %s->%s %s", msg.namespace_().c_str(), msg.destination_id().c_str(), msg.payload_utf8().c_str());
#endif

    SetDWBE(p_data, i_size);
    msg.SerializeWithCachedSizesToArray(p_data + PACKET_HEADER_LEN);

    vlc_mutex_locker locker(&lock);
    int i_ret = tls_Send(p_tls, p_data, PACKET_HEADER_LEN + i_size);
    delete[] p_data;
    if (i_ret == PACKET_HEADER_LEN + i_size)
        return VLC_SUCCESS;

    return VLC_EGENERIC;
}

/**
 * @brief Build a CastMessage to send to the Chromecast
 * @param namespace_ the message namespace
 * @param payload the payload
 * @param destinationId the destination identifier
 * @param payloadType the payload type (CastMessage_PayloadType_STRING or
 * CastMessage_PayloadType_BINARY
 * @return the generated CastMessage
 */
void vlc_renderer_sys::pushMessage(const std::string & namespace_,
                             const std::string & payload,
                             const std::string & destinationId,
                             castchannel::CastMessage_PayloadType payloadType)
{
    castchannel::CastMessage msg;

    msg.set_protocol_version(castchannel::CastMessage_ProtocolVersion_CASTV2_1_0);
    msg.set_namespace_(namespace_);
    msg.set_payload_type(payloadType);
    msg.set_source_id("sender-vlc");
    msg.set_destination_id(destinationId);
    if (payloadType == castchannel::CastMessage_PayloadType_STRING)
        msg.set_payload_utf8(payload);
    else // CastMessage_PayloadType_BINARY
        msg.set_payload_binary(payload);

    sendMessage(msg);
}

void vlc_renderer_sys::pushMediaPlayerMessage(const std::stringstream & payload) {
    assert(!appTransportId.empty());
    pushMessage( NAMESPACE_MEDIA, payload.str(), appTransportId );
}

void vlc_renderer_sys::msgPlayerSeek(const std::string & currentTime)
{
    assert(!mediaSessionId.empty());

    std::stringstream ss;
    ss << "{\"type\":\"SEEK\","
       <<  "\"currentTime\":" << currentTime << ","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << i_requestId++
       << "}";

    pushMediaPlayerMessage( ss );
}


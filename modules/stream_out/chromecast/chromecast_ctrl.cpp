/*****************************************************************************
 * chromecast_ctrl.cpp: Chromecast module for vlc
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

#include <vlc_input.h>
#include <vlc_playlist.h>

#include <cassert>
#include <cerrno>
#ifdef HAVE_POLL
# include <poll.h>
#endif

#include "../../misc/webservices/json.h"

#define PACKET_MAX_LEN 10 * 1024

// Media player Chromecast app id
#define APP_ID "CC1AD845" // Default media player aka DEFAULT_MEDIA_RECEIVER_APPLICATION_ID

static const int CHROMECAST_CONTROL_PORT = 8009;

/* deadline regarding pings sent from receiver */
#define PING_WAIT_TIME 6000
#define PING_WAIT_RETRIES 0
/* deadline regarding pong we expect after pinging the receiver */
#define PONG_WAIT_TIME 500
#define PONG_WAIT_RETRIES 2

#define CONTROL_CFG_PREFIX "chromecast-"

static const std::string NAMESPACE_DEVICEAUTH       = "urn:x-cast:com.google.cast.tp.deviceauth";
static const std::string NAMESPACE_CONNECTION       = "urn:x-cast:com.google.cast.tp.connection";
static const std::string NAMESPACE_HEARTBEAT        = "urn:x-cast:com.google.cast.tp.heartbeat";
static const std::string NAMESPACE_RECEIVER         = "urn:x-cast:com.google.cast.receiver";

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

static int SetInput(vlc_renderer *p_renderer, input_thread_t *p_input);

static void *ChromecastThread(void *data);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define HTTP_PORT_TEXT N_("HTTP port")
#define HTTP_PORT_LONGTEXT N_("This sets the HTTP port of the server " \
                              "used to stream the media to the Chromecast.")
#define MUXER_TEXT N_("Muxer")
#define MUXER_LONGTEXT N_("This sets the muxer used to stream to the Chromecast.")
#define MIME_TEXT N_("MIME content type")
#define MIME_LONGTEXT N_("This sets the media MIME content type sent to the Chromecast.")

vlc_module_begin ()
    set_shortname( N_("Chromecast") )
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_RENDERER)
    set_description( N_("Chromecast renderer") )
    set_capability( "renderer", 0 )
    add_shortcut("chromecast")
    add_integer(CONTROL_CFG_PREFIX "http-port", HTTP_PORT, HTTP_PORT_TEXT, HTTP_PORT_LONGTEXT, false)
    add_string(CONTROL_CFG_PREFIX "mime", "video/x-matroska", MIME_TEXT, MIME_LONGTEXT, false)
    add_string(CONTROL_CFG_PREFIX "mux", "avformat{mux=matroska}", MUXER_TEXT, MUXER_LONGTEXT, false)
    set_callbacks( Open, Close )

vlc_module_end ()

/*****************************************************************************
 * Open: connect to the Chromecast and initialize the sout
 *****************************************************************************/
int Open(vlc_object_t *p_module)
{
    vlc_renderer *p_renderer = reinterpret_cast<vlc_renderer*>(p_module);
    vlc_renderer_sys *p_sys = new(std::nothrow) vlc_renderer_sys( p_renderer );
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;
    p_renderer->p_sys = p_sys;

    char *psz_mux = var_InheritString( p_module, CONTROL_CFG_PREFIX "mux");
    if (psz_mux == NULL)
    {
        msg_Err( p_module, "Bad muxer provided");
        goto error;
    }
    p_sys->muxer = psz_mux;
    free(psz_mux);

    psz_mux = var_InheritString( p_module, CONTROL_CFG_PREFIX "mime");
    if (psz_mux == NULL)
    {
        msg_Err( p_module, "Bad MIME type provided");
        goto error;
    }
    p_sys->mime = psz_mux; /* TODO get the MIME type from the playlist/input ? */
    free(psz_mux);

    p_renderer->pf_set_input = SetInput;

    // Start the Chromecast event thread.
    if (vlc_clone(&p_sys->chromecastThread, ChromecastThread, p_module,
                  VLC_THREAD_PRIORITY_LOW))
    {
        msg_Err( p_module, "Could not start the Chromecast talking thread");
        goto error;
    }

    return VLC_SUCCESS;

error:
    delete p_sys;
    return VLC_EGENERIC;
}


/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
void Close(vlc_object_t *p_module)
{
    vlc_renderer *p_renderer = reinterpret_cast<vlc_renderer*>(p_module);
    vlc_renderer_sys *p_sys = p_renderer->p_sys;

    delete p_sys;
}

/**
 * @brief Build a CastMessage to send to the Chromecast
 * @param namespace_ the message namespace
 * @param payloadType the payload type (CastMessage_PayloadType_STRING or
 * CastMessage_PayloadType_BINARY
 * @param payload the payload
 * @param destinationId the destination idenifier
 * @return the generated CastMessage
 */
void vlc_renderer_sys::buildMessage(const std::string & namespace_,
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

/*****************************************************************************
 * vlc_renderer_sys: class definition
 *****************************************************************************/
vlc_renderer_sys::vlc_renderer_sys(vlc_renderer * const p_this)
 : p_module( VLC_OBJECT(p_this) )
 , p_input(NULL)
 , receiverState(RECEIVER_IDLE)
 , i_sock_fd(-1)
 , p_creds(NULL)
 , p_tls(NULL)
 , conn_status(CHROMECAST_DISCONNECTED)
 , cmd_status(NO_CMD_PENDING)
 , i_receiver_requestId(0)
 , i_requestId(0)
{
    vlc_mutex_init_recursive(&lock);
    vlc_cond_init(&loadCommandCond);
}

vlc_renderer_sys::~vlc_renderer_sys()
{
    InputUpdated( NULL );

    switch (getConnectionStatus())
    {
    case CHROMECAST_APP_STARTED:
        // Generate the close messages.
        msgReceiverClose(appTransportId);
        // ft
    case CHROMECAST_AUTHENTICATED:
        msgReceiverClose(DEFAULT_CHOMECAST_RECEIVER);
        // ft
    default:
        break;
    }

    vlc_cancel(chromecastThread);
    vlc_join(chromecastThread, NULL);

    disconnectChromecast();

    vlc_cond_destroy(&loadCommandCond);
    vlc_mutex_destroy(&lock);
}

static int SetInput(vlc_renderer *p_renderer, input_thread_t *p_input)
{
    vlc_renderer_sys *p_sys = p_renderer->p_sys;
    p_sys->InputUpdated( p_input );
    return VLC_SUCCESS;
}

void vlc_renderer_sys::InputUpdated( input_thread_t *p_input )
{
    vlc_renderer *p_renderer = reinterpret_cast<vlc_renderer*>( p_module );
    vlc_mutex_locker locker(&lock);
    msg_Dbg( p_module, "InputUpdated p_input:%p was:%p device:%s session:%s", (void*)p_input, (void*)this->p_input,
             p_renderer->target.psz_host, mediaSessionId.c_str() );

    if ( this->p_input == p_input )
        return;

    if( this->p_input != NULL )
    {
        var_SetString( this->p_input, "sout", NULL );
#if 0 /* the Chromecast doesn't work well with consecutives calls if stopped between them */
        if ( receiverState != RECEIVER_IDLE )
            msgPlayerStop();
#endif
    }

    this->p_input = p_input;

    if( this->p_input != NULL )
    {
        mutex_cleanup_push(&lock);
        while (conn_status != CHROMECAST_APP_STARTED && conn_status != CHROMECAST_CONNECTION_DEAD)
        {
            msg_Dbg( p_module, "InputUpdated waiting for Chromecast connection, current %d", conn_status);
            vlc_cond_wait(&loadCommandCond, &lock);
        }
        vlc_cleanup_pop();

        if (conn_status == CHROMECAST_CONNECTION_DEAD)
        {
            msg_Warn( p_module, "no Chromecast hook possible");
            return;
        }

        assert(!p_input->b_preparsing);

        int i_port = var_InheritInteger( p_module, CONTROL_CFG_PREFIX "http-port");

        std::stringstream ssout;
        ssout << '#';
        ssout << "cc_sout{http-port=" << i_port << ",mux=" << muxer << ",mime=" << mime << "}";

        msg_Dbg( p_module, "force sout to %s", ssout.str().c_str());
        var_SetString( this->p_input, "sout", ssout.str().c_str() );

        if ( receiverState == RECEIVER_IDLE )
        {
            // we cannot start a new load when the last one is still processing
            msgPlayerLoad();
            setPlayerStatus(CMD_LOAD_SENT);
        }
    }
}

/**
 * @brief Connect to the Chromecast
 * @return the opened socket file descriptor or -1 on error
 */
int vlc_renderer_sys::connectChromecast()
{
    vlc_renderer *p_renderer = reinterpret_cast<vlc_renderer*>(p_module);
    unsigned devicePort = p_renderer->target.i_port;
    if (devicePort == 0)
        devicePort = CHROMECAST_CONTROL_PORT;
    int fd = net_ConnectTCP( p_module, p_renderer->target.psz_host, devicePort);
    if (fd < 0)
        return -1;

    p_creds = vlc_tls_ClientCreate( p_module->p_parent );
    if (p_creds == NULL)
    {
        net_Close(fd);
        return -1;
    }

    p_tls = vlc_tls_ClientSessionCreateFD(p_creds, fd, p_renderer->target.psz_host,
                                               "tcps", NULL, NULL);

    if (p_tls == NULL)
    {
        net_Close(fd);
        vlc_tls_Delete(p_creds);
        return -1;
    }

    return fd;
}


/**
 * @brief Disconnect from the Chromecast
 */
void vlc_renderer_sys::disconnectChromecast()
{
    if (p_tls)
    {
        vlc_tls_Close(p_tls);
        vlc_tls_Delete(p_creds);
        p_tls = NULL;
        setConnectionStatus(CHROMECAST_DISCONNECTED);
        appTransportId = "";
        mediaSessionId = ""; // this session is not valid anymore
        setPlayerStatus(NO_CMD_PENDING);
        receiverState = RECEIVER_IDLE;
    }
}


/**
 * @brief Receive a data packet from the Chromecast
 * @param p_module the module to log with
 * @param b_msgReceived returns true if a message has been entirely received else false
 * @param i_payloadSize returns the payload size of the message received
 * @return the number of bytes received of -1 on error
 */
// Use here only C linkage and POD types as this function is a cancelation point.
extern "C" int recvPacket(vlc_object_t *p_module, bool &b_msgReceived,
                          uint32_t &i_payloadSize, int i_sock_fd, vlc_tls_t *p_tls,
                          unsigned *pi_received, uint8_t *p_data, bool *pb_pingTimeout,
                          int *pi_wait_delay, int *pi_wait_retries)
{
    struct pollfd ufd[1];
    ufd[0].fd = i_sock_fd;
    ufd[0].events = POLLIN;

    /* The Chromecast normally sends a PING command every 5 seconds or so.
     * If we do not receive one after 6 seconds, we send a PING.
     * If after this PING, we do not receive a PONG, then we consider the
     * connection as dead. */
    if (poll(ufd, 1, *pi_wait_delay) == 0)
    {
        if (*pb_pingTimeout)
        {
            if (!*pi_wait_retries)
            {
                msg_Err( p_module, "No PONG answer received from the Chromecast");
                return 0; // Connection died
            }
            (*pi_wait_retries)--;
        }
        else
        {
            /* now expect a pong */
            *pi_wait_delay = PONG_WAIT_TIME;
            *pi_wait_retries = PONG_WAIT_RETRIES;
            msg_Warn( p_module, "No PING received from the Chromecast, sending a PING");
        }
        *pb_pingTimeout = true;
    }
    else
    {
        *pb_pingTimeout = false;
        /* reset to default ping waiting */
        *pi_wait_delay = PING_WAIT_TIME;
        *pi_wait_retries = PING_WAIT_RETRIES;
    }

    int i_ret;

    /* Packet structure:
     * +------------------------------------+------------------------------+
     * | Payload size (uint32_t big endian) |         Payload data         |
     * +------------------------------------+------------------------------+ */
    while (*pi_received < PACKET_HEADER_LEN)
    {
        // We receive the header.
        i_ret = tls_Recv(p_tls, p_data + *pi_received, PACKET_HEADER_LEN - *pi_received);
        if (i_ret <= 0)
            return i_ret;
        *pi_received += i_ret;
    }

    // We receive the payload.

    // Get the size of the payload
    i_payloadSize = U32_AT( p_data );
    const uint32_t i_maxPayloadSize = PACKET_MAX_LEN - PACKET_HEADER_LEN;

    if (i_payloadSize > i_maxPayloadSize)
    {
        // Error case: the packet sent by the Chromecast is too long: we drop it.
        msg_Err( p_module, "Packet too long: droping its data");

        uint32_t i_size = i_payloadSize - (*pi_received - PACKET_HEADER_LEN);
        if (i_size > i_maxPayloadSize)
            i_size = i_maxPayloadSize;

        i_ret = tls_Recv(p_tls, p_data + PACKET_HEADER_LEN, i_size);
        if (i_ret <= 0)
            return i_ret;
        *pi_received += i_ret;

        if (*pi_received < i_payloadSize + PACKET_HEADER_LEN)
            return i_ret;

        *pi_received = 0;
        return -1;
    }

    // Normal case
    i_ret = tls_Recv(p_tls, p_data + *pi_received,
                     i_payloadSize - (*pi_received - PACKET_HEADER_LEN));
    if (i_ret <= 0)
        return i_ret;
    *pi_received += i_ret;

    if (*pi_received < i_payloadSize + PACKET_HEADER_LEN)
        return i_ret;

    assert(*pi_received == i_payloadSize + PACKET_HEADER_LEN);
    *pi_received = 0;
    b_msgReceived = true;
    return i_ret;
}

/**
 * @brief Process a message received from the Chromecast
 * @param msg the CastMessage to process
 * @return 0 if the message has been successfuly processed else -1
 */
void vlc_renderer_sys::processMessage(const castchannel::CastMessage &msg)
{
    const std::string & namespace_ = msg.namespace_();

#ifndef NDEBUG
    msg_Dbg( p_module, "processMessage: %s->%s %s", namespace_.c_str(), msg.destination_id().c_str(), msg.payload_utf8().c_str());
#endif

    if (namespace_ == NAMESPACE_DEVICEAUTH)
    {
        castchannel::DeviceAuthMessage authMessage;
        authMessage.ParseFromString(msg.payload_binary());

        if (authMessage.has_error())
        {
            msg_Err( p_module, "Authentification error: %d", authMessage.error().error_type());
        }
        else if (!authMessage.has_response())
        {
            msg_Err( p_module, "Authentification message has no response field");
        }
        else
        {
            vlc_mutex_locker locker(&lock);
            setConnectionStatus(CHROMECAST_AUTHENTICATED);
            msgConnect(DEFAULT_CHOMECAST_RECEIVER);
            msgReceiverGetStatus();
        }
    }
    else if (namespace_ == NAMESPACE_HEARTBEAT)
    {
        json_value *p_data = json_parse(msg.payload_utf8().c_str());
        std::string type((*p_data)["type"]);

        if (type == "PING")
        {
            msg_Dbg( p_module, "PING received from the Chromecast");
            msgPong();
        }
        else if (type == "PONG")
        {
            msg_Dbg( p_module, "PONG received from the Chromecast");
        }
        else
        {
            msg_Warn( p_module, "Heartbeat command not supported: %s", type.c_str());
        }

        json_value_free(p_data);
    }
    else if (namespace_ == NAMESPACE_RECEIVER)
    {
        json_value *p_data = json_parse(msg.payload_utf8().c_str());
        std::string type((*p_data)["type"]);

        if (type == "RECEIVER_STATUS")
        {
            json_value applications = (*p_data)["status"]["applications"];
            const json_value *p_app = NULL;

            vlc_mutex_locker locker(&lock);
            for (unsigned i = 0; i < applications.u.array.length; ++i)
            {
                std::string appId(applications[i]["appId"]);
                if (appId == APP_ID)
                {
                    const char *pz_transportId = applications[i]["transportId"];
                    if (pz_transportId != NULL)
                    {
                        appTransportId = std::string(pz_transportId);
                        p_app = &applications[i];
                    }
                    break;
                }
            }

            if ( p_app )
            {
                if (!appTransportId.empty()
                        && getConnectionStatus() == CHROMECAST_AUTHENTICATED)
                {
                    msgConnect(appTransportId);
                    setConnectionStatus(CHROMECAST_APP_STARTED);
                }
                else
                {
                    msgPlayerGetStatus();
                }
            }
            else
            {
                switch(getConnectionStatus())
                {
                /* If the app is no longer present */
                case CHROMECAST_APP_STARTED:
                    msg_Warn( p_module, "app is no longer present. closing");
                    msgReceiverClose(appTransportId);
                    setConnectionStatus(CHROMECAST_CONNECTION_DEAD);
                    break;

                case CHROMECAST_AUTHENTICATED:
                    msg_Dbg( p_module, "Chromecast was running no app, launch media_app");
                    appTransportId = "";
                    mediaSessionId = ""; // this session is not valid anymore
                    receiverState = RECEIVER_IDLE;
                    msgReceiverLaunchApp();
                    break;

                default:
                    break;
                }

            }
        }
        else if (type == "LAUNCH_ERROR")
        {
            json_value reason = (*p_data)["reason"];
            msg_Err( p_module, "Failed to start the MediaPlayer: %s",
                    (const char *)reason);
        }
        else
        {
            msg_Warn( p_module, "Receiver command not supported: %s",
                    msg.payload_utf8().c_str());
        }

        json_value_free(p_data);
    }
    else if (namespace_ == NAMESPACE_MEDIA)
    {
        json_value *p_data = json_parse(msg.payload_utf8().c_str());
        std::string type((*p_data)["type"]);

        if (type == "MEDIA_STATUS")
        {
            json_value status = (*p_data)["status"];
            msg_Dbg( p_module, "Player state: %s sessionId:%d",
                    status[0]["playerState"].operator const char *(),
                    (int)(json_int_t) status[0]["mediaSessionId"]);

            vlc_mutex_locker locker(&lock);
            receiver_state oldPlayerState = receiverState;
            std::string newPlayerState = status[0]["playerState"].operator const char *();

            if (newPlayerState == "IDLE")
                receiverState = RECEIVER_IDLE;
            else if (newPlayerState == "PLAYING")
                receiverState = RECEIVER_PLAYING;
            else if (newPlayerState == "BUFFERING")
                receiverState = RECEIVER_BUFFERING;
            else if (newPlayerState == "PAUSED")
                receiverState = RECEIVER_PAUSED;
            else if (!newPlayerState.empty())
                msg_Warn( p_module, "Unknown Chromecast state %s", newPlayerState.c_str());

            if (receiverState == RECEIVER_IDLE)
                mediaSessionId = ""; // this session is not valid anymore
            else
            {
                char session_id[32];
                if( snprintf( session_id, sizeof(session_id), "%" PRId64, (json_int_t) status[0]["mediaSessionId"] ) >= (int)sizeof(session_id) )
                {
                    msg_Err( p_module, "snprintf() truncated string for mediaSessionId" );
                    session_id[sizeof(session_id) - 1] = '\0';
                }
                if (!mediaSessionId.empty() && session_id[0] && mediaSessionId != session_id) {
                    msg_Warn( p_module, "different mediaSessionId detected %s was %s", mediaSessionId.c_str(), this->mediaSessionId.c_str());
                }

                mediaSessionId = session_id;
            }

            if (receiverState != oldPlayerState)
            {
#ifndef NDEBUG
                msg_Dbg( p_module, "change Chromecast player state from %d to %d", oldPlayerState, receiverState);
#endif
                switch( receiverState )
                {
                case RECEIVER_PLAYING:
                    /* TODO reset demux PCR ? */
                    setPlayerStatus(CMD_PLAYBACK_SENT);
                    break;

                case RECEIVER_PAUSED:
#ifndef NDEBUG
                    msg_Dbg( p_module, "Playback paused");
#endif
                    break;

                case RECEIVER_IDLE:
                    if ( p_input != NULL )
                        setPlayerStatus(NO_CMD_PENDING);
                    break;
                }
            }

            if ( cmd_status != CMD_LOAD_SENT && receiverState == RECEIVER_IDLE && p_input != NULL )
            {
                msg_Dbg( p_module, "the device missed the LOAD command");
                msgPlayerLoad();
                setPlayerStatus(CMD_LOAD_SENT);
            }
        }
        else if (type == "LOAD_FAILED")
        {
            msg_Err( p_module, "Media load failed");
            vlc_mutex_locker locker(&lock);
            /* close the app to restart it */
            if (getConnectionStatus() == CHROMECAST_APP_STARTED)
                msgReceiverClose(appTransportId);
            else
                msgReceiverGetStatus();
        }
        else if (type == "INVALID_REQUEST")
        {
            msg_Dbg( p_module, "We sent an invalid request reason:%s", (*p_data)["reason"].operator const char *());
        }
        else
        {
            msg_Warn( p_module, "Media command not supported: %s",
                    msg.payload_utf8().c_str());
        }

        json_value_free(p_data);
    }
    else if (namespace_ == NAMESPACE_CONNECTION)
    {
        json_value *p_data = json_parse(msg.payload_utf8().c_str());
        std::string type((*p_data)["type"]);
        json_value_free(p_data);

        if (type == "CLOSE")
        {
            msg_Warn( p_module, "received close message");
            InputUpdated( NULL );
            vlc_mutex_locker locker(&lock);
            setConnectionStatus(CHROMECAST_CONNECTION_DEAD);
        }
        else
        {
            msg_Warn( p_module, "Connection command not supported: %s",
                    type.c_str());
        }
    }
    else
    {
        msg_Err( p_module, "Unknown namespace: %s", msg.namespace_().c_str());
    }
}

/*****************************************************************************
 * Message preparation
 *****************************************************************************/
void vlc_renderer_sys::msgAuth()
{
    castchannel::DeviceAuthMessage authMessage;
    authMessage.mutable_challenge();

    buildMessage(NAMESPACE_DEVICEAUTH, authMessage.SerializeAsString(),
                 DEFAULT_CHOMECAST_RECEIVER, castchannel::CastMessage_PayloadType_BINARY);
}


void vlc_renderer_sys::msgPing()
{
    std::string s("{\"type\":\"PING\"}");
    buildMessage( NAMESPACE_HEARTBEAT, s );
}


void vlc_renderer_sys::msgPong()
{
    std::string s("{\"type\":\"PONG\"}");
    buildMessage( NAMESPACE_HEARTBEAT, s );
}

void vlc_renderer_sys::msgConnect(const std::string & destinationId)
{
    std::string s("{\"type\":\"CONNECT\"}");
    buildMessage( NAMESPACE_CONNECTION, s, destinationId );
}


void vlc_renderer_sys::msgReceiverClose(std::string destinationId)
{
    std::string s("{\"type\":\"CLOSE\"}");
    buildMessage( NAMESPACE_CONNECTION, s, destinationId );
    if (appTransportId != destinationId)
        setConnectionStatus( CHROMECAST_DISCONNECTED );
    else
    {
        appTransportId = "";
        setConnectionStatus( CHROMECAST_AUTHENTICATED );
    }
}

void vlc_renderer_sys::msgReceiverGetStatus()
{
    std::stringstream ss;
    ss << "{\"type\":\"GET_STATUS\","
       <<  "\"requestId\":" << i_receiver_requestId++ << "}";

    buildMessage( NAMESPACE_RECEIVER, ss.str() );
}

void vlc_renderer_sys::msgReceiverLaunchApp()
{
    std::stringstream ss;
    ss << "{\"type\":\"LAUNCH\","
       <<  "\"appId\":\"" << APP_ID << "\","
       <<  "\"requestId\":" << i_receiver_requestId++ << "}";

    buildMessage( NAMESPACE_RECEIVER, ss.str() );
}

void vlc_renderer_sys::msgPlayerGetStatus()
{
    std::stringstream ss;
    ss << "{\"type\":\"GET_STATUS\","
       <<  "\"requestId\":" << i_requestId++
       << "}";

    pushMediaPlayerMessage( ss );
}

void vlc_renderer_sys::msgPlayerLoad()
{
    char *psz_mime = var_InheritString(p_module, CONTROL_CFG_PREFIX "mime");
    if (psz_mime == NULL)
        return;

    std::stringstream ss;
    ss << "{\"type\":\"LOAD\","
       <<  "\"media\":{\"contentId\":\"http://" << serverIP << ":"
           << var_InheritInteger(p_module, CONTROL_CFG_PREFIX"http-port")
           << "/stream\","
       <<             "\"streamType\":\"LIVE\","
       <<             "\"contentType\":\"" << std::string(psz_mime) << "\"},"
       <<  "\"requestId\":" << i_requestId++ << "}";

    free(psz_mime);

    pushMediaPlayerMessage( ss );
}

void vlc_renderer_sys::msgPlayerPlay()
{
    assert(!mediaSessionId.empty());

    std::stringstream ss;
    ss << "{\"type\":\"PLAY\","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << i_requestId++
       << "}";

    pushMediaPlayerMessage( ss );
}

void vlc_renderer_sys::msgPlayerStop()
{
    assert(!mediaSessionId.empty());

    std::stringstream ss;
    ss << "{\"type\":\"STOP\","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << i_requestId++
       << "}";

    pushMediaPlayerMessage( ss );
}

void vlc_renderer_sys::msgPlayerPause()
{
    assert(!mediaSessionId.empty());

    std::stringstream ss;
    ss << "{\"type\":\"PAUSE\","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << i_requestId++
       << "}";

    pushMediaPlayerMessage( ss );
}

void vlc_renderer_sys::msgPlayerSetVolume(float f_volume)
{
    assert(!mediaSessionId.empty());

    if ( f_volume < 0.0 || f_volume > 1.0)
        return;

    std::stringstream ss;
    ss << "{\"type\":\"SET_VOLUME\","
       <<  "\"volume\":{\"level\":" << f_volume << "},"
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << i_requestId++
       << "}";

    pushMediaPlayerMessage( ss );
}

void vlc_renderer_sys::msgPlayerSetMute(bool b_mute)
{
    assert(!mediaSessionId.empty());

    std::stringstream ss;
    ss << "{\"type\":\"SET_VOLUME\","
       <<  "\"volume\":{\"muted\":" << ( b_mute ? "true" : "false" ) << "},"
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << i_requestId++
       << "}";

    pushMediaPlayerMessage( ss );
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
    msg_Dbg( p_module, "sendMessage: %s->%s %s", msg.namespace_().c_str(), msg.destination_id().c_str(), msg.payload_utf8().c_str());
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

void vlc_renderer_sys::pushMediaPlayerMessage(const std::stringstream & payload) {
    assert(!appTransportId.empty());
    buildMessage( NAMESPACE_MEDIA, payload.str(), appTransportId );
}

/*****************************************************************************
 * Chromecast thread
 *****************************************************************************/
static void* ChromecastThread(void* p_data)
{
    int canc;
    vlc_renderer *p_intf = reinterpret_cast<vlc_renderer*>(p_data);
    vlc_renderer_sys *p_sys = p_intf->p_sys;
    p_sys->setConnectionStatus( CHROMECAST_DISCONNECTED );

    p_sys->i_sock_fd = p_sys->connectChromecast();
    if (p_sys->i_sock_fd < 0)
    {
        canc = vlc_savecancel();
        msg_Err(p_intf, "Could not connect the Chromecast");
        vlc_mutex_lock(&p_sys->lock);
        p_sys->setConnectionStatus(CHROMECAST_CONNECTION_DEAD);
        vlc_mutex_unlock(&p_sys->lock);
        vlc_restorecancel(canc);
        return NULL;
    }

    char psz_localIP[NI_MAXNUMERICHOST];
    if (net_GetSockAddress(p_sys->i_sock_fd, psz_localIP, NULL))
    {
        canc = vlc_savecancel();
        msg_Err(p_intf, "Cannot get local IP address");
        vlc_mutex_lock(&p_sys->lock);
        p_sys->setConnectionStatus(CHROMECAST_CONNECTION_DEAD);
        vlc_mutex_unlock(&p_sys->lock);
        vlc_restorecancel(canc);
        return NULL;
    }

    canc = vlc_savecancel();
    p_sys->serverIP = psz_localIP;

    vlc_mutex_lock(&p_sys->lock);
    p_sys->setConnectionStatus(CHROMECAST_TLS_CONNECTED);
    vlc_mutex_unlock(&p_sys->lock);

    p_sys->msgAuth();
    vlc_restorecancel(canc);

    while (1)
    {
        p_sys->handleMessages();

        vlc_mutex_locker locker(&p_sys->lock);
        if ( p_sys->getConnectionStatus() == CHROMECAST_CONNECTION_DEAD )
            break;
    }

    return NULL;
}

void vlc_renderer_sys::handleMessages()
{
    unsigned i_received = 0;
    uint8_t p_packet[PACKET_MAX_LEN];
    bool b_pingTimeout = false;

    int i_waitdelay = PING_WAIT_TIME;
    int i_retries = PING_WAIT_RETRIES;

    bool b_msgReceived = false;
    uint32_t i_payloadSize = 0;
    int i_ret = recvPacket( p_module, b_msgReceived, i_payloadSize, i_sock_fd,
                           p_tls, &i_received, p_packet, &b_pingTimeout,
                           &i_waitdelay, &i_retries);

    int canc = vlc_savecancel();
    // Not cancellation-safe part.

#if defined(_WIN32)
    if ((i_ret < 0 && WSAGetLastError() != WSAEWOULDBLOCK) || (i_ret == 0))
#else
    if ((i_ret < 0 && errno != EAGAIN) || i_ret == 0)
#endif
    {
        msg_Err( p_module, "The connection to the Chromecast died (receiving).");
        vlc_mutex_locker locker(&lock);
        setConnectionStatus(CHROMECAST_CONNECTION_DEAD);
        vlc_restorecancel(canc);
        return;
    }

    if (b_pingTimeout)
    {
        msgPing();
        msgReceiverGetStatus();
    }

    if (b_msgReceived)
    {
        castchannel::CastMessage msg;
        msg.ParseFromArray(p_packet + PACKET_HEADER_LEN, i_payloadSize);
        processMessage(msg);
    }

    vlc_restorecancel(canc);
}

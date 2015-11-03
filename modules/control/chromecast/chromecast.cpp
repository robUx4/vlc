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

#include <cassert>
#include <sstream>
#include <queue>

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include "cast_channel.pb.h"

#include "../../misc/webservices/json.h"

#ifdef HAVE_MICRODNS
# include <microdns/microdns.h>

static const char MDNS_CHROMECAST[] = "._googlecast._tcp.local";
#endif

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);
static int DemuxOpen(vlc_object_t *);
static void DemuxClose(vlc_object_t *);
static int SoutOpen(vlc_object_t *);
static void SoutClose(vlc_object_t *);
static int PlaylistEvent( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int InputEvent( vlc_object_t *, char const *,
                       vlc_value_t, vlc_value_t, void * );
static void *chromecastThread(void *data);
static int connectChromecast(intf_thread_t *p_intf);
static void disconnectChromecast(intf_thread_t *p_intf);

#define CONTROL_CFG_PREFIX "chromecast-"
#define SOUT_CFG_PREFIX    "sout-chromecast-"
#define SOUT_INTF_ADDRESS  "sout-chromecast-intf"

// Status
enum connection_status {
    CHROMECAST_DISCONNECTED,
    CHROMECAST_TLS_CONNECTED,
    CHROMECAST_AUTHENTICATED,
    CHROMECAST_APP_STARTED,
    CHROMECAST_DEAD,
};

enum player_status {
    PLAYER_IDLE,
    PLAYER_LOAD_SENT,
    PLAYER_PLAYBACK_SENT,
};

#define PACKET_MAX_LEN 10 * 1024
#define PACKET_HEADER_LEN 4

// Media player Chromecast app id
#define MEDIA_RECEIVER_APP_ID "CC1AD845" // Default media player aka DEFAULT_MEDIA_RECEIVER_APPLICATION_ID
static const std::string DEFAULT_CHOMECAST_RECEIVER = "receiver-0";

#define HTTP_PORT               8010

/* deadline regarding pings sent from receiver */
#define PING_WAIT_TIME 6000
#define PING_WAIT_RETRIES 0
/* deadline regarding pong we expect after pinging the receiver */
#define PONG_WAIT_TIME 500
#define PONG_WAIT_RETRIES 2
#define TIMEOUT_LOAD_CMD  (6 * CLOCK_FREQ)
#define TIMEOUT_MDNS_IP   (4 * CLOCK_FREQ)

static const std::string NAMESPACE_DEVICEAUTH = "urn:x-cast:com.google.cast.tp.deviceauth";
static const std::string NAMESPACE_CONNECTION = "urn:x-cast:com.google.cast.tp.connection";
static const std::string NAMESPACE_HEARTBEAT  = "urn:x-cast:com.google.cast.tp.heartbeat";
static const std::string NAMESPACE_RECEIVER   = "urn:x-cast:com.google.cast.receiver";
/* see https://developers.google.com/cast/docs/reference/messages */
static const std::string NAMESPACE_MEDIA      = "urn:x-cast:com.google.cast.media";

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    intf_sys_t(intf_thread_t *intf)
        :p_intf(intf)
        ,p_input(NULL)
        ,devicePort(8009)
#ifdef HAVE_MICRODNS
        ,microdns_ctx(NULL)
#endif
        ,time_offset(0)
        ,time_start_play(-1)
        ,playback_time_ref(-1.0)
        ,canPause(false)
        ,i_sock_fd(-1)
        ,p_creds(NULL)
        ,p_tls(NULL)
        ,conn_status(CHROMECAST_DISCONNECTED)
        ,play_status(PLAYER_IDLE)
        ,i_supportedMediaCommands(15)
        ,f_seektime(-1.0)
        ,i_app_requestId(0)
        ,i_requestId(0)
    {
        p_interrupt = vlc_interrupt_create();
    }

    ~intf_sys_t()
    {
#ifdef HAVE_MICRODNS
        mdns_cleanup(microdns_ctx);
#endif
        vlc_interrupt_destroy(p_interrupt);
    }

    mtime_t getPlaybackTime() const {
        if (time_start_play == -1)
            return 0;
        if (playerState == "PLAYING")
            return mdate() - time_start_play + time_offset;
        return time_offset;
    }

    double getPlaybackPosition(mtime_t i_length) const {
        if( i_length > 0 && time_start_play > 0)
            return (double) getPlaybackTime() / (double)( i_length );
        return 0.0;
    }

    void setCanPause(bool canPause) {
        vlc_mutex_locker locker(&lock);
        this->canPause = canPause;
    }

    bool isBuffering() {
        vlc_mutex_locker locker(&lock);
        return conn_status == CHROMECAST_DEAD || playerState == "BUFFERING";
    }

    bool seekTo(mtime_t pos);

    void sendPlayerCmd();

    intf_thread_t  * const p_intf;
    input_thread_t *p_input;
    uint16_t       devicePort;
    std::string    deviceIP;
    std::string    serverIP;
    std::string    mime;
#ifdef HAVE_MICRODNS
    struct mdns_ctx *microdns_ctx;
    mtime_t         i_timeout;
    std::string     nameChromecast; /* name we're looking for */
#endif

    std::string appTransportId;
    std::string mediaSessionId;
    std::string playerState;

    mtime_t     time_offset;
    mtime_t     time_start_play;
    double      playback_time_ref;
    bool        canPause;

    int i_sock_fd;
    vlc_tls_creds_t *p_creds;
    vlc_tls_t *p_tls;

    enum connection_status conn_status;
    enum player_status     play_status;
    int                    i_supportedMediaCommands;
    double                 f_seektime;
    std::string            currentTime; /* position of seeking requested */

    vlc_interrupt_t *p_interrupt;
    vlc_mutex_t  lock;
    vlc_cond_t   loadCommandCond; /* TODO not needed anymore ? */
    vlc_cond_t   seekCommandCond;
    vlc_thread_t chromecastThread;


    int sendMessages();

    void msgAuth();
    void msgPing();
    void msgPong();
    void msgConnect(const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER);

    void msgReceiverLaunchApp();
    void msgReceiverClose();
    void msgReceiverGetStatus();
    void msgPlayerGetStatus();

private:
    int sendMessage(castchannel::CastMessage &msg);

    void pushMessage(const std::string & namespace_,
                      const std::string & payload,
                      const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER,
                      castchannel::CastMessage_PayloadType payloadType = castchannel::CastMessage_PayloadType_STRING);

    void pushMediaPlayerMessage(const std::stringstream & payload) {
        assert(!appTransportId.empty());
        pushMessage(NAMESPACE_MEDIA, payload.str(), appTransportId);
    }

    unsigned i_app_requestId;
    unsigned i_requestId;
    std::queue<castchannel::CastMessage> messagesToSend;

    void msgPlayerLoad();
    void msgPlayerStop();
    void msgPlayerPlay();
    void msgPlayerPause();
    void msgPlayerSeek(std::string & currentTime);
};

#define IP_TEXT N_("Chromecast IP address")
#define IP_LONGTEXT N_("This sets the IP adress of the Chromecast receiver.")
#define TARGET_TEXT N_("Chromecast Name")
#define TARGET_LONGTEXT N_("This sets the name of the Chromecast receiver.")
#define HTTP_PORT_TEXT N_("HTTP port")
#define HTTP_PORT_LONGTEXT N_("This sets the HTTP port of the server " \
                              "used to stream the media to the Chromecast.")
#define MIME_TEXT N_("MIME content type")
#define MIME_LONGTEXT N_("This sets the media MIME content type sent to the Chromecast.")
#define MUXER_TEXT N_("Output muxer address")
#define MUXER_LONGTEXT N_("Output muxer chromecast interface.")

vlc_module_begin ()
    set_shortname( N_("Chromecast") )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_description( N_("Chromecast interface") )
    set_capability( "interface", 0 )
    add_shortcut("chromecast")
    add_string(CONTROL_CFG_PREFIX "ip", "", IP_TEXT, IP_LONGTEXT, false)
#ifdef HAVE_MICRODNS
    add_string(CONTROL_CFG_PREFIX "target", "", TARGET_TEXT, TARGET_LONGTEXT, false)
#endif
    add_integer(CONTROL_CFG_PREFIX "http-port", HTTP_PORT, HTTP_PORT_TEXT, HTTP_PORT_LONGTEXT, false)
    add_string(CONTROL_CFG_PREFIX "mime", "video/x-matroska", MIME_TEXT, MIME_LONGTEXT, false)
    set_callbacks( Open, Close )

    add_submodule()
        set_shortname( "cc_demux" )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_DEMUX )
        set_description( N_( "chromecast demux wrapper" ) )
        set_capability( "demux", 0 )
        add_shortcut( "cc_demux" )
        set_callbacks( DemuxOpen, DemuxClose )

    add_submodule()
        set_shortname( "cc_sout" )
        set_category(CAT_SOUT)
        set_subcategory(SUBCAT_SOUT_STREAM)
        set_description( N_( "chromecast sout wrapper" ) )
        set_capability("sout stream", 0)
        add_shortcut("cc_sout")
        set_callbacks( SoutOpen, SoutClose )
        add_integer(SOUT_CFG_PREFIX "http-port", HTTP_PORT, HTTP_PORT_TEXT, HTTP_PORT_LONGTEXT, false)
        add_string(SOUT_CFG_PREFIX "mime", "video/x-matroska", MIME_TEXT, MIME_LONGTEXT, false)
        add_string(SOUT_CFG_PREFIX "mux", "avformat{mux=matroska}", MUXER_TEXT, MUXER_LONGTEXT, false)

vlc_module_end ()

int Open(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = reinterpret_cast<intf_thread_t*>(p_this);
    intf_sys_t *p_sys = new(std::nothrow) intf_sys_t(p_intf);
    char *psz_mime;
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    char *psz_ipChromecast = var_InheritString(p_intf, CONTROL_CFG_PREFIX "ip");
    if (psz_ipChromecast != NULL)
    {
        p_sys->deviceIP = psz_ipChromecast;
        free(psz_ipChromecast);
    }
    else
#ifdef HAVE_MICRODNS
    if (mdns_init(&p_sys->microdns_ctx, MDNS_ADDR_IPV4, MDNS_PORT) >= 0)
    {
        char *psz_nameChromecast = var_InheritString(p_intf, CONTROL_CFG_PREFIX "target");
        if (psz_nameChromecast != NULL)
        {
            p_sys->nameChromecast = psz_nameChromecast;
            free(psz_nameChromecast);
        }
    }
    if (p_sys->microdns_ctx == NULL && p_sys->deviceIP.empty())
#endif /* HAVE_MICRODNS */
    {
        msg_Err(p_intf, "No Chromecast receiver IP/Name provided");
        goto error;
    }

    psz_mime = var_InheritString(p_intf, CONTROL_CFG_PREFIX "mime");
    if (psz_mime == NULL)
    {
        msg_Err(p_intf, "Bad MIME type provided");
        goto error;
    }
    p_sys->mime = psz_mime; /* TODO get the MIME type from the playlist/input ? */
    free(psz_mime);

    vlc_mutex_init(&p_sys->lock);
    vlc_cond_init(&p_sys->loadCommandCond);
    vlc_cond_init(&p_sys->seekCommandCond);

    p_intf->p_sys = p_sys;

    // Start the Chromecast event thread.
    if (vlc_clone(&p_sys->chromecastThread, chromecastThread, p_intf,
                  VLC_THREAD_PRIORITY_LOW))
    {
        msg_Err(p_intf, "Could not start the Chromecast talking thread");
        goto error;
    }

    var_AddCallback( pl_Get(p_intf), "input-current", PlaylistEvent, p_intf );

    return VLC_SUCCESS;

error:
    delete p_sys;
    return VLC_EGENERIC;
}

void Close(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = reinterpret_cast<intf_thread_t*>(p_this);
    intf_sys_t *p_sys = p_intf->p_sys;

    p_sys->msgReceiverClose();

    var_DelCallback( pl_Get(p_intf), "input-current", PlaylistEvent, p_intf );

    if( p_sys->p_input != NULL )
        var_DelCallback( p_sys->p_input, "intf-event", InputEvent, p_intf );

    vlc_cancel(p_sys->chromecastThread);
    vlc_join(p_sys->chromecastThread, NULL);

    vlc_mutex_destroy(&p_sys->lock);
    vlc_cond_destroy(&p_sys->seekCommandCond);
    vlc_cond_destroy(&p_sys->loadCommandCond);

    disconnectChromecast(p_intf);

    delete p_sys;
}

static int PlaylistEvent( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t val, void *p_data )
{
    intf_thread_t *p_intf = static_cast<intf_thread_t *>(p_data);
    intf_sys_t *p_sys = p_intf->p_sys;
    input_thread_t *p_input = static_cast<input_thread_t *>(val.p_address);

    VLC_UNUSED(p_this);
    VLC_UNUSED(psz_var);

    vlc_mutex_locker locker(&p_sys->lock);
    assert(p_sys->p_input != p_input);

    msg_Dbg(p_intf, "PlaylistEvent input changed");

    if( p_sys->p_input != NULL )
    {
        assert( p_sys->p_input == oldval.p_address );
        var_DelCallback( p_sys->p_input, "intf-event", InputEvent, p_intf );
        var_SetAddress( p_sys->p_input->p_parent, SOUT_INTF_ADDRESS, NULL );
    }

    p_sys->p_input = p_input;

    if( p_input != NULL )
    {
        var_AddCallback( p_input, "intf-event", InputEvent, p_intf );

        assert(!p_input->b_preparsing);

        var_Create( p_input->p_parent, SOUT_INTF_ADDRESS, VLC_VAR_ADDRESS );
        var_SetAddress( p_input->p_parent, SOUT_INTF_ADDRESS, p_intf );

        std::stringstream ss;
        ss << "#cc_sout{http-port=" << var_InheritInteger(p_intf, CONTROL_CFG_PREFIX "http-port")
           << ",mux=avformat{mux=matroska}"
           << ",mime=" << p_sys->mime << "}";
        var_SetString( p_input, "sout", ss.str().c_str() );

        var_SetString( p_input, "demux-filter", "cc_demux" );
    }

    return VLC_SUCCESS;
}

void intf_sys_t::sendPlayerCmd()
{
    if (conn_status != CHROMECAST_APP_STARTED)
        return;

    if (!p_input)
        return;

    assert(!p_input->b_preparsing);

    switch( var_GetInteger( p_input, "state" ) )
    {
    case OPENING_S:
        if (!mediaSessionId.empty()) {
            msgPlayerStop();
            mediaSessionId = "";
        }
        playback_time_ref = -1.0;
        msgPlayerLoad();
        play_status = PLAYER_LOAD_SENT;
        break;
    case PLAYING_S:
        if (!mediaSessionId.empty()) {
            msgPlayerPlay();
            play_status = PLAYER_PLAYBACK_SENT;
        } else if (play_status == PLAYER_IDLE) {
            msgPlayerLoad();
            play_status = PLAYER_LOAD_SENT;
        }
        break;
    case PAUSE_S:
        if (!mediaSessionId.empty()) {
            msgPlayerPause();
            play_status = PLAYER_PLAYBACK_SENT;
        } else if (play_status == PLAYER_IDLE) {
            msgPlayerLoad();
            play_status = PLAYER_LOAD_SENT;
        }
        break;
    case END_S:
        if (!mediaSessionId.empty()) {
            msgPlayerStop();

            /* TODO reset the sout as we'll need another one for the next load */
            var_SetString( p_input, "sout", NULL );
            play_status = PLAYER_IDLE;
        }
        break;
    default:
        //msgClose();
        break;
    }
}

bool intf_sys_t::seekTo(mtime_t pos)
{
    vlc_mutex_locker locker(&lock);
    if (conn_status == CHROMECAST_DEAD)
        return false;

    assert(playback_time_ref != -1.0);
    //msgPlayerStop();

    f_seektime = /* playback_time_ref + */ ( double( pos ) / 1000000.0 );
    currentTime = std::to_string( f_seektime );
    msg_Dbg( p_intf, "Seeking to %s playback_time:%f", currentTime.c_str(), playback_time_ref);
    msgPlayerSeek( currentTime );

    return true;
}

static int InputEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t val, void *p_data )
{
    input_thread_t *p_input = reinterpret_cast<input_thread_t*>(p_this);
    intf_thread_t *p_intf = static_cast<intf_thread_t*>(p_data);
    intf_sys_t *p_sys = p_intf->p_sys;

    VLC_UNUSED(psz_var);
    VLC_UNUSED(oldval);

    assert(p_input == p_sys->p_input);

    switch( val.i_int )
    {
    case INPUT_EVENT_STATE:
        {
            msg_Info(p_this, "playback state changed %d", (int)var_GetInteger( p_input, "state" ));
            vlc_mutex_locker locker(&p_sys->lock);
            p_sys->sendPlayerCmd();
        }
        break;
    }

    return VLC_SUCCESS;
}


/**
 * @brief Connect to the Chromecast
 * @param p_intf the intf_thread_t structure
 * @return the opened socket file descriptor or -1 on error
 */
static int connectChromecast(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    int fd = net_ConnectTCP(p_intf, p_sys->deviceIP.c_str(), p_sys->devicePort);
    if (fd < 0)
        return -1;

    p_sys->p_creds = vlc_tls_ClientCreate(VLC_OBJECT(p_intf));
    if (p_sys->p_creds == NULL)
    {
        net_Close(fd);
        return -1;
    }

    p_sys->p_tls = vlc_tls_ClientSessionCreate(p_sys->p_creds, fd, p_sys->deviceIP.c_str(),
                                               "tcps", NULL, NULL);

    if (p_sys->p_tls == NULL)
    {
        vlc_tls_Delete(p_sys->p_creds);
        return -1;
    }

    return fd;
}


/**
 * @brief Disconnect from the Chromecast
 */
static void disconnectChromecast(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;

    if (p_sys->p_tls)
    {
        vlc_tls_SessionDelete(p_sys->p_tls);
        vlc_tls_Delete(p_sys->p_creds);
        p_sys->p_tls = NULL;
        p_sys->conn_status = CHROMECAST_DISCONNECTED;
    }
}


/**
 * @brief Send a message to the Chromecast
 * @param msg the CastMessage to send
 * @return the number of bytes sent or -1 on error
 */
int intf_sys_t::sendMessage(castchannel::CastMessage &msg)
{
    uint32_t i_size = msg.ByteSize();
    uint32_t i_sizeNetwork = hton32(i_size);

    uint8_t *p_data = new(std::nothrow) uint8_t[PACKET_HEADER_LEN + i_size];
    if (p_data == NULL)
        return -1;

#ifndef NDEBUG
    msg_Dbg(p_intf, "sendMessage: %s payload:%s", msg.namespace_().c_str(), msg.payload_utf8().c_str());
#endif

    memcpy(p_data, &i_sizeNetwork, PACKET_HEADER_LEN);
    msg.SerializeWithCachedSizesToArray(p_data + PACKET_HEADER_LEN);

    int i_ret = tls_Send(p_tls, p_data, PACKET_HEADER_LEN + i_size);
    delete[] p_data;

    return i_ret;
}


/**
 * @brief Send all the messages in the pending queue to the Chromecast
 * @param msg the CastMessage to send
 * @return the number of bytes sent or -1 on error
 */
int intf_sys_t::sendMessages()
{
    int i_ret = 0;
    while (!messagesToSend.empty())
    {
        unsigned i_retSend = sendMessage(messagesToSend.front());
        if (i_retSend <= 0)
            return i_retSend;

        messagesToSend.pop();
        i_ret += i_retSend;
    }

    return i_ret;
}




/**
 * @brief Receive a data packet from the Chromecast
 * @param p_intf the intf_thread_t structure
 * @param b_msgReceived returns true if a message has been entirely received else false
 * @param i_payloadSize returns the payload size of the message received
 * @return the number of bytes received of -1 on error
 */
// Use here only C linkage and POD types as this function is a cancelation point.
extern "C" int recvPacket(intf_thread_t *p_intf, bool &b_msgReceived,
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
                msg_Err(p_intf, "No PONG answer received from the Chromecast");
                return 0; // Connection died
            }
            (*pi_wait_retries)--;
        }
        else
        {
            /* now expect a pong */
            *pi_wait_delay = PONG_WAIT_TIME;
            *pi_wait_retries = PONG_WAIT_RETRIES;
            msg_Warn(p_intf, "No PING received from the Chromecast, sending a PING");
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
        msg_Err(p_intf, "Packet too long: droping its data");

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
 * @param p_intf the intf_thread_t structure
 * @param msg the CastMessage to process
 * @return 0 if the message has been successfuly processed else -1
 */
static int processMessage(intf_thread_t *p_intf, const castchannel::CastMessage &msg)
{
    int i_ret = 0;
    intf_sys_t *p_sys = p_intf->p_sys;
    std::string namespace_ = msg.namespace_();

#ifndef NDEBUG
    msg_Dbg(p_intf,"processMessage: %s payload:%s", namespace_.c_str(), msg.payload_utf8().c_str());
#endif

    if (namespace_ == NAMESPACE_DEVICEAUTH)
    {
        castchannel::DeviceAuthMessage authMessage;
        authMessage.ParseFromString(msg.payload_binary());

        if (authMessage.has_error())
        {
            msg_Err(p_intf, "Authentification error: %d", authMessage.error().error_type());
            i_ret = -1;
        }
        else if (!authMessage.has_response())
        {
            msg_Err(p_intf, "Authentification message has no response field");
            i_ret = -1;
        }
        else
        {
            vlc_mutex_locker locker(&p_sys->lock);
            p_sys->conn_status = CHROMECAST_AUTHENTICATED;
            p_sys->msgConnect();
            p_sys->msgReceiverGetStatus();
        }
    }
    else if (namespace_ == NAMESPACE_HEARTBEAT)
    {
        json_value *p_data = json_parse(msg.payload_utf8().c_str());
        std::string type((*p_data)["type"]);

        if (type == "PING")
        {
            msg_Dbg(p_intf, "PING received from the Chromecast");
            p_sys->msgPong();
#if 0
            if (!p_sys->appTransportId.empty())
                p_sys->msgPlayerGetStatus();
#endif
        }
        else if (type == "PONG")
        {
            msg_Dbg(p_intf, "PONG received from the Chromecast");
        }
        else
        {
            msg_Err(p_intf, "Heartbeat command not supported: %s", type.c_str());
            i_ret = -1;
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

            vlc_mutex_locker locker(&p_sys->lock);
            for (unsigned i = 0; i < applications.u.array.length; ++i)
            {
                std::string appId(applications[i]["appId"]);
                if (appId == MEDIA_RECEIVER_APP_ID)
                {
                    p_app = &applications[i];
                    p_sys->appTransportId = std::string(applications[i]["transportId"]);
                    break;
                }
                else if (!appId.empty())
                {
                    /* the app running is not compatible with VLC, launch the compatible one */
                    msg_Dbg(p_intf, "Chromecast was running app:%s", appId.c_str());
                    p_sys->appTransportId = "";
                    p_sys->msgReceiverLaunchApp();
                }
            }

            if ( p_app )
            {
                if (!p_sys->appTransportId.empty()
                        && p_sys->conn_status == CHROMECAST_AUTHENTICATED)
                {
                    p_sys->conn_status = CHROMECAST_APP_STARTED;
                    p_sys->msgConnect(p_sys->appTransportId);
                    p_sys->sendPlayerCmd();
                    vlc_cond_signal(&p_sys->loadCommandCond);
                }
            }
            else
            {
                switch(p_sys->conn_status)
                {
                /* If the app is no longer present */
                case CHROMECAST_APP_STARTED:
                    msg_Warn(p_intf, "app is no longer present. closing");
                    p_sys->msgReceiverClose();
                    vlc_cond_signal(&p_sys->loadCommandCond);
                    // ft
                default:
                    break;
                }

            }
        }
        else
        {
            msg_Err(p_intf, "Receiver command not supported: %s",
                    msg.payload_utf8().c_str());
            i_ret = -1;
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
            msg_Dbg(p_intf, "Player state: %s sessionId:%d",
                    status[0]["playerState"].operator const char *(),
                    (int)(json_int_t) status[0]["mediaSessionId"]);

            vlc_mutex_locker locker(&p_sys->lock);
            std::string mediaSessionId = std::to_string((json_int_t) status[0]["mediaSessionId"]);
            if (!mediaSessionId.empty() && mediaSessionId != p_sys->mediaSessionId) {
                msg_Warn(p_intf, "different mediaSessionId detected %s", mediaSessionId.c_str());
            }
            p_sys->mediaSessionId = mediaSessionId;
            p_sys->i_supportedMediaCommands = status[0]["supportedMediaCommands"].operator json_int_t();

            std::string playerState = p_sys->playerState;
            p_sys->playerState = status[0]["playerState"].operator const char *();
            if (p_sys->playerState != playerState)
            {
                if (p_sys->playerState == "PLAYING")
                {
                    /* TODO reset demux PCR ? */
                    p_sys->time_start_play = mdate();
                    p_sys->playback_time_ref = double( status[0]["currentTime"] );
                    msg_Dbg(p_intf, "Playback starting with an offset of %f", p_sys->playback_time_ref);
                }
                else if (playerState == "PLAYING")
                {
                    /* playing stopped for now */
                    p_sys->time_offset += mdate() - p_sys->time_start_play;
                    p_sys->time_start_play = 0;
                    /* TODO after seeking we should reset the offset */
                }
            }
            if (p_sys->playerState == "BUFFERING" && p_sys->f_seektime != -1.0)
            {
                if ( int64_t( 10.0 * p_sys->f_seektime ) ==
                     int64_t( 10.0 * double( status[0]["currentTime"] ) ) )
                {
                    msg_Dbg(p_intf, "Chromecast ready to receive seeked data");
                    p_sys->currentTime = "";
                    vlc_cond_broadcast( &p_sys->seekCommandCond );
                }
                else
                {
                    msg_Dbg(p_intf, "Chromecast not ready to use seek data from %f got %f", p_sys->f_seektime, double( status[0]["currentTime"] ));
                }
            }
            if (p_sys->play_status == PLAYER_LOAD_SENT)
                p_sys->sendPlayerCmd();
        }
        else if (type == "LOAD_FAILED")
        {
            msg_Err(p_intf, "Media load failed");
            vlc_mutex_locker locker(&p_sys->lock);
            p_sys->msgReceiverClose();
            vlc_cond_signal(&p_sys->loadCommandCond);
        }
        else
        {
            msg_Err(p_intf, "Media command not supported: %s",
                    msg.payload_utf8().c_str());
            i_ret = -1;
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
            msg_Warn(p_intf, "received close message");
            vlc_mutex_locker locker(&p_sys->lock);
            p_sys->conn_status = CHROMECAST_DEAD;
            vlc_cond_signal(&p_sys->loadCommandCond);
        }
        else
        {
            msg_Err(p_intf, "Connection command not supported: %s",
                    type.c_str());
            i_ret = -1;
        }
    }
    else
    {
        msg_Err(p_intf, "Unknown namespace: %s", msg.namespace_().c_str());
        i_ret = -1;
    }

    return i_ret;
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
void intf_sys_t::pushMessage(const std::string & namespace_,
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

    messagesToSend.push(msg);
}

/*****************************************************************************
 * Message preparation
 *****************************************************************************/
void intf_sys_t::msgAuth()
{
    castchannel::DeviceAuthMessage authMessage;
    authMessage.mutable_challenge();

    pushMessage(NAMESPACE_DEVICEAUTH, authMessage.SerializeAsString(),
                 DEFAULT_CHOMECAST_RECEIVER, castchannel::CastMessage_PayloadType_BINARY);
}


void intf_sys_t::msgPing()
{
    std::string s("{\"type\":\"PING\"}");
    pushMessage(NAMESPACE_HEARTBEAT, s);
}


void intf_sys_t::msgPong()
{
    std::string s("{\"type\":\"PONG\"}");
    pushMessage(NAMESPACE_HEARTBEAT, s);
}

void intf_sys_t::msgConnect(const std::string & destinationId)
{
    std::string s("{\"type\":\"CONNECT\"}");
    pushMessage(NAMESPACE_CONNECTION, s, destinationId);
}

void intf_sys_t::msgReceiverClose()
{
    if (appTransportId.empty())
        return;

    std::string s("{\"type\":\"CLOSE\"}");
    pushMessage(NAMESPACE_CONNECTION, s, appTransportId);

    appTransportId = "";
    conn_status = CHROMECAST_TLS_CONNECTED;
}

void intf_sys_t::msgReceiverLaunchApp()
{
    std::stringstream ss;
    ss << "{\"type\":\"LAUNCH\","
       <<  "\"appId\":\"" << MEDIA_RECEIVER_APP_ID << "\","
       <<  "\"requestId\":" << i_app_requestId++ << "}";

    pushMessage(NAMESPACE_RECEIVER, ss.str());
}

void intf_sys_t::msgReceiverGetStatus()
{
    std::stringstream ss;
    ss << "{\"type\":\"GET_STATUS\","
       <<  "\"requestId\":" << i_app_requestId++ << "}";

    pushMessage(NAMESPACE_RECEIVER, ss.str());
}

void intf_sys_t::msgPlayerGetStatus()
{
    std::stringstream ss;
    ss << "{\"type\":\"GET_STATUS\","
       <<  "\"requestId\":" << i_requestId++ << "}";

    pushMediaPlayerMessage( ss );
}

void intf_sys_t::msgPlayerLoad()
{
    intf_sys_t *p_sys = p_intf->p_sys;

    /* TODO: extract the metadata from p_sys->p_input */

    std::stringstream ss;
    ss << "{\"type\":\"LOAD\","
       <<  "\"autoplay\":\"false\","
       <<  "\"media\":{\"contentId\":\"http://" << serverIP << ":"
           << var_InheritInteger(p_intf, CONTROL_CFG_PREFIX "http-port")
           << "/stream\","
       <<             "\"streamType\":\"LIVE\","
       <<             "\"contentType\":\"" << mime << "\"},"
       <<  "\"requestId\":" << i_requestId++ << "}";

    pushMediaPlayerMessage( ss );
}

void intf_sys_t::msgPlayerStop()
{
    assert(!mediaSessionId.empty());

    std::stringstream ss;
    ss << "{\"type\":\"STOP\","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << i_requestId++ << "}";

    pushMediaPlayerMessage( ss );
}

void intf_sys_t::msgPlayerPlay()
{
    assert(!mediaSessionId.empty());

    std::stringstream ss;
    ss << "{\"type\":\"PLAY\","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << i_requestId++ << "}";

    pushMediaPlayerMessage( ss );
}

void intf_sys_t::msgPlayerPause()
{
    assert(!mediaSessionId.empty());

    std::stringstream ss;
    ss << "{\"type\":\"PAUSE\","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << i_requestId++ << "}";

    pushMediaPlayerMessage( ss );
}

void intf_sys_t::msgPlayerSeek(std::string & currentTime)
{
    assert(!mediaSessionId.empty());

    std::stringstream ss;
    ss << "{\"type\":\"SEEK\","
       <<  "\"currentTime\":" << currentTime << ","
        <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << i_requestId++ << "}";

    pushMediaPlayerMessage( ss );
}


#ifdef HAVE_MICRODNS
static bool mdnsShouldStop(void *p_callback_cookie)
{
    intf_thread_t *p_intf = reinterpret_cast<intf_thread_t*>(p_callback_cookie);
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_testcancel();

    return !p_sys->deviceIP.empty() || mdate() > p_sys->i_timeout;
}

static void mdnsCallback(void *p_callback_cookie, int i_status, const struct rr_entry *p_entry)
{
    intf_thread_t *p_intf = reinterpret_cast<intf_thread_t*>(p_callback_cookie);
    intf_sys_t *p_sys = p_intf->p_sys;

    if (i_status < 0)
    {
        char err_str[128];
        if (mdns_strerror(i_status, err_str, sizeof(err_str)) == 0)
        {
            msg_Dbg(p_intf, "mDNS lookup error: %s", err_str);
        }
    }
    else if (p_entry != NULL && p_entry->next != NULL)
    {
        std::string deviceName = p_entry->next->name;
        if (deviceName.length() >= strlen(MDNS_CHROMECAST))
        {
            std::string serviceName = deviceName.substr(deviceName.length() - strlen(MDNS_CHROMECAST));
            deviceName = deviceName.substr(0, deviceName.length() - strlen(MDNS_CHROMECAST));
            if (serviceName == MDNS_CHROMECAST &&
                    (p_sys->nameChromecast.empty() ||
                     !strncasecmp(deviceName.c_str(), p_sys->nameChromecast.c_str(), p_sys->nameChromecast.length())))
            {
                while (p_entry != NULL)
                {
                    if (p_entry->type == RR_A)
                        p_sys->deviceIP = p_entry->data.A.addr_str;
                    else if (p_entry->type == RR_AAAA)
                        p_sys->deviceIP = p_entry->data.AAAA.addr_str;
                    else if (p_entry->type == RR_SRV)
                        p_sys->devicePort = p_entry->data.SRV.port;
                    p_entry = p_entry->next;
                }
                msg_Dbg(p_intf, "Found %s:%d for target %s", p_sys->deviceIP.c_str(), p_sys->devicePort, deviceName.c_str());
            }
        }
    }
}
#endif /* HAVE_MICRODNS */

/*****************************************************************************
 * Chromecast thread
 *****************************************************************************/
static void* chromecastThread(void* p_this)
{
    int canc;
    intf_thread_t *p_intf = reinterpret_cast<intf_thread_t*>(p_this);
    intf_sys_t *p_sys = p_intf->p_sys;

    unsigned i_received = 0;
    uint8_t p_packet[PACKET_MAX_LEN];
    bool b_pingTimeout = false;

    int i_waitdelay = PING_WAIT_TIME;
    int i_retries = PING_WAIT_RETRIES;

    vlc_interrupt_set(p_sys->p_interrupt);

#ifdef HAVE_MICRODNS
    if (p_sys->microdns_ctx != NULL)
    {
        int err;
        p_sys->i_timeout = mdate() + TIMEOUT_MDNS_IP;
        if ((err = mdns_listen(p_sys->microdns_ctx, MDNS_CHROMECAST+1, 2, &mdnsShouldStop, &mdnsCallback, p_intf)) < 0)
        {
            char err_str[128];
            canc = vlc_savecancel();
            if (mdns_strerror(err, err_str, sizeof(err_str)) == 0)
                msg_Err(p_intf, "Failed to look for the target Name: %s", err_str);
            vlc_mutex_lock(&p_sys->lock);
            p_sys->conn_status = CHROMECAST_DEAD;
            vlc_cond_signal(&p_sys->loadCommandCond);
            vlc_mutex_unlock(&p_sys->lock);
            vlc_restorecancel(canc);
            return NULL;
        }
    }
#endif /* HAVE_MICRODNS */

    p_sys->i_sock_fd = connectChromecast(p_intf);
    if (p_sys->i_sock_fd < 0)
    {
        canc = vlc_savecancel();
        msg_Err(p_intf, "Could not connect the Chromecast");
        vlc_mutex_lock(&p_sys->lock);
        p_sys->conn_status = CHROMECAST_DEAD;
        vlc_cond_signal(&p_sys->loadCommandCond);
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
        p_sys->conn_status = CHROMECAST_DEAD;
        vlc_cond_signal(&p_sys->loadCommandCond);
        vlc_mutex_unlock(&p_sys->lock);
        vlc_restorecancel(canc);
        return NULL;
    }

    canc = vlc_savecancel();
    p_sys->serverIP = psz_localIP;

    p_sys->conn_status = CHROMECAST_TLS_CONNECTED;

    p_sys->msgAuth();
    p_sys->sendMessages();
    vlc_restorecancel(canc);

    while (1)
    {
        bool b_msgReceived = false;
        uint32_t i_payloadSize = 0;
        int i_ret = recvPacket(p_intf, b_msgReceived, i_payloadSize, p_sys->i_sock_fd,
                               p_sys->p_tls, &i_received, p_packet, &b_pingTimeout,
                               &i_waitdelay, &i_retries);

        canc = vlc_savecancel();
        // Not cancellation-safe part.

#if defined(_WIN32)
        if ((i_ret < 0 && WSAGetLastError() != WSAEWOULDBLOCK) || (i_ret == 0))
#else
        if ((i_ret < 0 && errno != EAGAIN) || i_ret == 0)
#endif
        {
            msg_Err(p_intf, "The connection to the Chromecast died (receiving).");
            vlc_mutex_locker locker(&p_sys->lock);
            p_sys->conn_status = CHROMECAST_DEAD;
            vlc_cond_signal(&p_sys->loadCommandCond);
            break;
        }

        if (b_pingTimeout)
        {
            p_sys->msgPing();
            p_sys->msgReceiverGetStatus();
        }

        if (b_msgReceived)
        {
            castchannel::CastMessage msg;
            msg.ParseFromArray(p_packet + PACKET_HEADER_LEN, i_payloadSize);
            processMessage(p_intf, msg);
        }

        // Send the answer messages if there is any.
        i_ret = p_sys->sendMessages();
#if defined(_WIN32)
        if ((i_ret < 0 && WSAGetLastError() != WSAEWOULDBLOCK))
#else
        if ((i_ret < 0 && errno != EAGAIN))
#endif
        {
            msg_Err(p_intf, "The connection to the Chromecast died (sending).");
            vlc_mutex_locker locker(&p_sys->lock);
            p_sys->conn_status = CHROMECAST_DEAD;
            vlc_cond_signal(&p_sys->loadCommandCond);
        }

        if ( p_sys->conn_status == CHROMECAST_DEAD )
            break;

        vlc_restorecancel(canc);
    }

    return NULL;
}

struct demux_sys_t
{
    demux_sys_t(demux_t *demux, intf_thread_t *intf)
        :p_demux(demux)
        ,p_intf(intf)
        ,i_length(-1)
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
        return p_intf->p_sys->getPlaybackTime();
    }

    double getPlaybackPosition() {
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
        return p_intf->p_sys->seekTo(i_pos);
    }

    void setLength(mtime_t length) {
        this->i_length = length;
    }

    int Demux() {
        /* TODO hold the data while seeking */
        vlc_mutex_lock(&p_demux->p_sys->p_intf->p_sys->lock);
        /* wait until the client is buffering for seeked data */
        if (p_demux->p_sys->p_intf->p_sys->f_seektime != -1.0)
        {
            msg_Dbg(p_demux, "waiting for Chromecast seek");
            vlc_cond_wait(&p_demux->p_sys->p_intf->p_sys->seekCommandCond, &p_intf->p_sys->lock);
            msg_Dbg(p_demux, "finished waiting for Chromecast seek");

            int i_ret = source_Control( DEMUX_SET_TIME, mtime_t( p_demux->p_sys->p_intf->p_sys->f_seektime * 1000000.0 ) );
            p_demux->p_sys->p_intf->p_sys->f_seektime = -1.0;
            vlc_mutex_unlock(&p_demux->p_sys->p_intf->p_sys->lock);
            if (i_ret != VLC_SUCCESS)
                return 0;
            return 1;
        }
        vlc_mutex_unlock(&p_demux->p_sys->p_intf->p_sys->lock);

        return p_demux->p_source->pf_demux( p_demux->p_source );
    }

protected:
    demux_t       *p_demux;
    intf_thread_t *p_intf;
    mtime_t       i_length;
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
        if (!p_sys->seekTo( pos ))
        {
            msg_Err( p_demux, "failed to seek to %f", pos );
            return VLC_EGENERIC;
        }
#if TODO
        ret = p_demux->p_source->pf_control( p_demux->p_source, i_query, args );
        if (ret == VLC_SUCCESS) {

        }
#endif
        return ret;
    }

    case DEMUX_SET_TIME:
    {
        int ret = VLC_SUCCESS;
        va_list ap;

        va_copy( ap, args );
        mtime_t pos = va_arg( ap, mtime_t );
        va_end( ap );
        if (!p_sys->seekTo( pos ))
        {
            msg_Err( p_demux, "failed to seek to time %" PRId64, pos );
            return VLC_EGENERIC;
        }
#if TODO
        ret = p_demux->p_source->pf_control( p_demux->p_source, i_query, args );
        if (ret == VLC_SUCCESS) {

        }
#endif
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

struct sout_stream_sys_t
{
    sout_stream_sys_t(sout_stream_t *stream, intf_thread_t *intf, sout_stream_t *sout)
        :p_out(sout)
        ,p_intf(intf)
        ,p_stream(stream)
    {
        assert(p_intf != NULL);
        vlc_object_hold(p_intf);
    }

    ~sout_stream_sys_t()
    {
        sout_StreamChainDelete(p_out, p_out);
        vlc_object_release(p_intf);
    }

    int sendBlock(sout_stream_id_sys_t *id, block_t *p_buffer);

public:
    bool isFinishedPlaying() const {
        /* check if the Chromecast to be done playing */
        return p_intf->p_sys->isBuffering();
    }

    sout_stream_t * const p_out;

protected:
    intf_thread_t * const p_intf;
    sout_stream_t * const p_stream;
};

/*****************************************************************************
 * Sout callbacks
 *****************************************************************************/
static sout_stream_id_sys_t *Add(sout_stream_t *p_stream, const es_format_t *p_fmt)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    return p_sys->p_out->pf_add(p_sys->p_out, p_fmt);
}


static void Del(sout_stream_t *p_stream, sout_stream_id_sys_t *id)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    p_sys->p_out->pf_del(p_sys->p_out, id);
}


static int Send(sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                block_t *p_buffer)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    return p_sys->sendBlock(id, p_buffer);
}

int sout_stream_sys_t::sendBlock(sout_stream_id_sys_t *id,
                                 block_t *p_buffer)
{
    /* hold the data while seeking */
    vlc_mutex_lock(&p_intf->p_sys->lock);
    /* wait until the client is buffering for seeked data */
    if (p_intf->p_sys->f_seektime != -1.0)
    {
        msg_Dbg(p_stream, "waiting for Chromecast seek");
        vlc_cond_wait(&p_intf->p_sys->seekCommandCond, &p_intf->p_sys->lock);
        msg_Dbg(p_stream, "finished waiting for Chromecast seek");
    }
    vlc_mutex_unlock(&p_intf->p_sys->lock);

    return p_out->pf_send(p_out, id, p_buffer);
}

static int Control(sout_stream_t *p_stream, int i_query, va_list args)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if (i_query == SOUT_STREAM_EMPTY)
    {
        bool *b = va_arg( args, bool * );
        *b = p_sys->isFinishedPlaying();
        return VLC_SUCCESS;
    }

    return p_sys->p_out->pf_control(p_sys->p_out, i_query, args);
}

static const char *const ppsz_sout_options[] = {
    "http-port", "mux", "mime", NULL
};

int SoutOpen(vlc_object_t *p_this)
{
    sout_stream_t *p_stream = reinterpret_cast<sout_stream_t*>(p_this);
    char *psz_var_mux = NULL, *psz_var_mime = NULL;
    intf_thread_t *p_intf = NULL;
    sout_stream_sys_t *p_sys = NULL;
    sout_stream_t *p_sout = NULL;
    std::stringstream ss;

    config_ChainParse(p_stream, SOUT_CFG_PREFIX, ppsz_sout_options, p_stream->p_cfg);

    p_intf = static_cast<intf_thread_t*>(var_InheritAddress(p_stream, SOUT_INTF_ADDRESS));
    if (p_intf == NULL) {
        msg_Err(p_stream, "Missing the control interface to work");
        goto error;
    }

    psz_var_mux = var_InheritString(p_stream, SOUT_CFG_PREFIX "mux");
    if (psz_var_mux == NULL || !psz_var_mux[0])
        goto error;
    psz_var_mime = var_InheritString(p_stream, SOUT_CFG_PREFIX "mime");
    if (psz_var_mime == NULL || !psz_var_mime[0])
        goto error;

    ss << "standard{dst=:" << var_InheritInteger(p_stream, SOUT_CFG_PREFIX "http-port") << "/stream"
       << ",mux=" << psz_var_mux
       << ",access=simplehttpd{mime=" << psz_var_mime << "}}";

    p_sout = sout_StreamChainNew( p_stream->p_sout, ss.str().c_str(), NULL, NULL);
    if (p_sout == NULL) {
        msg_Dbg(p_stream, "could not create sout chain:%s", ss.str().c_str());
        goto error;
    }

    p_sys = new(std::nothrow) sout_stream_sys_t(p_stream, p_intf, p_sout);
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_stream->pf_add     = Add;
    p_stream->pf_del     = Del;
    p_stream->pf_send    = Send;
    p_stream->pf_control = Control;

    p_stream->p_sys = p_sys;
    free(psz_var_mux);
    free(psz_var_mime);
    return VLC_SUCCESS;

error:
    delete p_sys;
    sout_StreamChainDelete(p_sout, p_sout);
    free(psz_var_mux);
    free(psz_var_mime);
    return VLC_EGENERIC;
}

void SoutClose(vlc_object_t *p_this)
{
    sout_stream_t *p_sout = reinterpret_cast<sout_stream_t*>(p_this);
    delete p_sout->p_sys;
}

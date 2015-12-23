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

#include <vlc_access.h>
#include <vlc_input.h>
#include <vlc_playlist.h>
#include <vlc_threads.h>
#include <vlc_url.h>

#include <cassert>
#include <cerrno>
#ifdef HAVE_POLL
# include <poll.h>
#endif

#include "../../misc/webservices/json.h"

static const vlc_fourcc_t DEFAULT_TRANSCODE_AUDIO = VLC_CODEC_MP3;
static const vlc_fourcc_t DEFAULT_TRANSCODE_VIDEO = VLC_CODEC_H264;

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

static int CurrentChanged( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int InputEvent( vlc_object_t *, char const *,
                       vlc_value_t, vlc_value_t, void * );
static int MuteChanged( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int VolumeChanged( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static void *ChromecastThread(void *data);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define IP_TEXT N_("Chromecast IP address")
#define IP_LONGTEXT N_("This sets the IP adress of the Chromecast receiver.")
#define HTTP_PORT_TEXT N_("HTTP port")
#define HTTP_PORT_LONGTEXT N_("This sets the HTTP port of the server " \
                              "used to stream the media to the Chromecast.")
#define MUXER_TEXT N_("Muxer")
#define MUXER_LONGTEXT N_("This sets the muxer used to stream to the Chromecast.")
#define MIME_TEXT N_("MIME content type")
#define MIME_LONGTEXT N_("This sets the media MIME content type sent to the Chromecast.")

vlc_module_begin ()
    set_shortname( N_("Chromecast") )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_description( N_("Chromecast interface") )
    set_capability( "interface", 0 )
    add_shortcut("chromecast")
    add_string(CONTROL_CFG_PREFIX "addr", "", IP_TEXT, IP_LONGTEXT, false)
    add_integer(CONTROL_CFG_PREFIX "http-port", HTTP_PORT, HTTP_PORT_TEXT, HTTP_PORT_LONGTEXT, false)
    add_string(CONTROL_CFG_PREFIX "mime", "video/x-matroska", MIME_TEXT, MIME_LONGTEXT, false)
    add_string(CONTROL_CFG_PREFIX "mux", "avformat{mux=matroska}", MUXER_TEXT, MUXER_LONGTEXT, false)
    set_callbacks( Open, Close )

vlc_module_end ()

/*****************************************************************************
 * Open: connect to the Chromecast and initialize the sout
 *****************************************************************************/
int Open(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = reinterpret_cast<intf_thread_t*>(p_this);
    intf_sys_t *p_sys = new(std::nothrow) intf_sys_t(p_intf);
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    playlist_t *p_playlist = pl_Get( p_intf );
    std::stringstream receiver_addr;
    char *psz_ipChromecast = var_InheritString(p_intf, CONTROL_CFG_PREFIX "addr");

    if (psz_ipChromecast == NULL)
        msg_Info(p_intf, "No Chromecast receiver IP/Name provided");
    else
    {
        vlc_url_t url;
        vlc_UrlParse(&url, psz_ipChromecast);
        free(psz_ipChromecast);
        if (url.psz_host && url.psz_host[0])
        {
            int i_port = url.i_port ? url.i_port : CHROMECAST_CONTROL_PORT;
            receiver_addr << url.psz_host << ':' << i_port;
        }
        vlc_UrlClean(&url);
    }

    char *psz_mux = var_InheritString(p_intf, CONTROL_CFG_PREFIX "mux");
    if (psz_mux == NULL)
    {
        msg_Err(p_intf, "Bad muxer provided");
        goto error;
    }
    p_sys->muxer = psz_mux; /* TODO get the MIME type from the playlist/input ? */
    free(psz_mux);

    psz_mux = var_InheritString(p_intf, CONTROL_CFG_PREFIX "mime");
    if (psz_mux == NULL)
    {
        msg_Err(p_intf, "Bad MIME type provided");
        goto error;
    }
    p_sys->mime = psz_mux; /* TODO get the MIME type from the playlist/input ? */
    free(psz_mux);

    p_intf->p_sys = p_sys;

    var_AddCallback( p_playlist, "input-prepare", CurrentChanged, p_intf );
    var_AddCallback( p_playlist, "mute", MuteChanged, p_intf );
    var_AddCallback( p_playlist, "volume", VolumeChanged, p_intf );

    p_sys->ipChangedEvent( receiver_addr.str().c_str() );

    return VLC_SUCCESS;

error:
    delete p_sys;
    return VLC_EGENERIC;
}


/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
void Close(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = reinterpret_cast<intf_thread_t*>(p_this);
    intf_sys_t *p_sys = p_intf->p_sys;

    playlist_t *p_playlist = pl_Get( p_intf );
    var_DelCallback( p_playlist, "input-prepare", CurrentChanged, p_intf );
    var_DelCallback( p_playlist, "mute", MuteChanged, p_intf );
    var_DelCallback( p_playlist, "volume", VolumeChanged, p_intf );

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

void intf_sys_t::pushMediaPlayerMessage(const std::stringstream & payload) {
    assert(!appTransportId.empty());
    pushMessage( NAMESPACE_MEDIA, payload.str(), appTransportId );
}


/*****************************************************************************
 * intf_sys_t: class definition
 *****************************************************************************/
intf_sys_t::intf_sys_t(intf_thread_t * const p_this)
    :p_stream(p_this)
    ,p_input(NULL)
    ,devicePort(CHROMECAST_CONTROL_PORT)
    ,receiverState(RECEIVER_IDLE)
    ,canDisplay(DISPLAY_UNKNOWN)
    ,currentStopped(true)
    ,p_tls(NULL)
    ,conn_status(CHROMECAST_DISCONNECTED)
    ,cmd_status(NO_CMD_PENDING)
    ,i_receiver_requestId(0)
    ,i_requestId(0)
    ,i_sout_id(0)
{
    vlc_mutex_init(&lock);
    vlc_cond_init(&loadCommandCond);
}

intf_sys_t::~intf_sys_t()
{
    switch (getConnectionStatus())
    {
    case CHROMECAST_APP_STARTED:
        // Generate the close messages.
        msgReceiverClose(appTransportId);
        // ft
    case CHROMECAST_AUTHENTICATED:
        msgReceiverClose(DEFAULT_CHOMECAST_RECEIVER);
        // Send the just added close messages.
        sendMessages();
        // ft
    default:
        break;
    }

    ipChangedEvent( NULL );

    vlc_cond_destroy(&loadCommandCond);
    vlc_mutex_destroy(&lock);
}

void intf_sys_t::ipChangedEvent(const char *psz_new_ip)
{
    if (psz_new_ip == NULL)
        psz_new_ip = "";

    vlc_url_t url;
    vlc_UrlParse(&url, psz_new_ip);
    if (url.psz_host)
        psz_new_ip = url.psz_host;
    else
        psz_new_ip = "";

    if (deviceIP != psz_new_ip)
    {
        msg_Dbg(p_stream,"ipChangedEvent '%s' from '%s'", psz_new_ip, deviceIP.c_str());
        if ( !deviceIP.empty() )
        {
            /* disconnect the current Chromecast */
            msgReceiverClose(appTransportId);

            vlc_cancel(chromecastThread);
            vlc_join(chromecastThread, NULL);

            disconnectChromecast();
        }

        /* connect the new Chromecast (if needed) */
        deviceIP = psz_new_ip;
        devicePort = url.i_port ? url.i_port : CHROMECAST_CONTROL_PORT;

        InputUpdated( NULL );

        if ( !deviceIP.empty() )
        {
            // Start the new Chromecast event thread.
            if (vlc_clone(&chromecastThread, ChromecastThread, p_stream,
                          VLC_THREAD_PRIORITY_LOW))
            {
                msg_Err(p_stream, "Could not start the Chromecast talking thread");
            }
        }
    }
    vlc_UrlClean(&url);
}

static int MuteChanged( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t val, void *p_data )
{
    VLC_UNUSED( p_this );
    VLC_UNUSED( psz_var );
    VLC_UNUSED( oldval );
    intf_thread_t *p_intf = static_cast<intf_thread_t *>(p_data);
    intf_sys_t *p_sys = p_intf->p_sys;

    if (!p_sys->mediaSessionId.empty())
        p_sys->msgPlayerSetMute( val.b_bool );

    return VLC_SUCCESS;
}

static int VolumeChanged( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t val, void *p_data )
{
    VLC_UNUSED( p_this );
    VLC_UNUSED( psz_var );
    VLC_UNUSED( oldval );
    intf_thread_t *p_intf = static_cast<intf_thread_t *>(p_data);
    intf_sys_t *p_sys = p_intf->p_sys;

    if ( !p_sys->mediaSessionId.empty() )
        p_sys->msgPlayerSetVolume( val.f_float );

    return VLC_SUCCESS;
}

static int CurrentChanged( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t val, void *p_data )
{
    intf_thread_t *p_intf = static_cast<intf_thread_t *>(p_data);
    intf_sys_t *p_sys = p_intf->p_sys;
    input_thread_t *p_input = static_cast<input_thread_t *>(val.p_address);

    VLC_UNUSED(p_this);
    VLC_UNUSED(psz_var);

    assert( p_sys->p_input == NULL || p_sys->p_input == oldval.p_address );

    p_sys->InputUpdated( p_input );

    return VLC_SUCCESS;
}

bool intf_sys_t::canDecodeVideo( const es_format_t *p_es ) const
{
    if (p_es->i_codec == VLC_CODEC_H264 || p_es->i_codec == VLC_CODEC_VP8)
        return true;
    return false;
}

bool intf_sys_t::canDecodeAudio( const es_format_t *p_es ) const
{
    if (p_es->i_codec == VLC_CODEC_VORBIS ||
        p_es->i_codec == VLC_CODEC_MP4A ||
        p_es->i_codec == VLC_FOURCC('h', 'a', 'a', 'c') ||
        p_es->i_codec == VLC_FOURCC('l', 'a', 'a', 'c') ||
        p_es->i_codec == VLC_FOURCC('s', 'a', 'a', 'c') ||
        p_es->i_codec == VLC_CODEC_MPGA ||
        p_es->i_codec == VLC_CODEC_MP3 ||
        p_es->i_codec == VLC_CODEC_A52 ||
        p_es->i_codec == VLC_CODEC_EAC3)
        return true;
    return false;
}

void intf_sys_t::unplugOutputRedirection()
{
    msg_Dbg( p_stream, "unplug output redirection from input %s", input_GetItem( p_input )->psz_name );
        var_SetString( p_input, "sout", NULL );
}

void intf_sys_t::plugOutputRedirection()
{
    msg_Dbg( p_stream, "plug output redirection on input %s", input_GetItem( p_input )->psz_name );
        msg_Dbg(p_stream, "force sout to %s", s_sout.c_str());
        var_SetString( p_input, "sout", s_sout.c_str() );
}

void intf_sys_t::InputUpdated( input_thread_t *p_input )
{
    vlc_mutex_lock(&lock);
    msg_Dbg( p_stream, "%ld InputUpdated p_input:%p was:%p playlist_Status:%d", GetCurrentThreadId(), (void*)p_input, (void*)this->p_input, playlist_Status( pl_Get(p_stream) ) );

    if (deviceIP.empty())
    {
        /* we will connect when we start the thread */
        p_input = NULL;
    }

    if ( this->p_input == p_input )
    {
        vlc_mutex_unlock(&lock);
        return;
    }

    if( this->p_input != NULL )
    {
        vlc_mutex_unlock(&lock);
        var_DelCallback( this->p_input, "intf-event", InputEvent, p_stream );
        vlc_mutex_lock(&lock);
        unplugOutputRedirection();
    }

    this->p_input = p_input;

    if( this->p_input != NULL )
    {
        var_AddCallback( p_input, "intf-event", InputEvent, p_stream );

        mutex_cleanup_push(&lock);
        while (!deviceIP.empty() && canDisplay == DISPLAY_UNKNOWN && conn_status != CHROMECAST_CONNECTION_DEAD)
        {
            msg_Dbg(p_stream, "InputUpdated waiting for Chromecast connection, current %d", conn_status);
            vlc_cond_wait(&loadCommandCond, &lock);
        }
        vlc_cleanup_pop();

        if (conn_status == CHROMECAST_CONNECTION_DEAD)
        {
            msg_Warn(p_stream, "no Chromecast hook possible");
            vlc_mutex_unlock(&lock);
            return;
        }

        assert(!p_input->b_preparsing);

        canRemux = false;
        canDoDirect = false;
        vlc_fourcc_t i_codec_video = 0, i_codec_audio = 0;

        input_item_t * p_item = input_GetItem(p_input);
        if ( p_item )
        {
            canRemux = true;
            for (int i=0; i<p_item->i_es; ++i)
            {
                es_format_t *p_es = p_item->es[i];
                if (p_es->i_cat == AUDIO_ES)
                {
                    if (!canDecodeAudio( p_es ))
                    {
                        msg_Dbg( p_stream, "can't remux audio track %d codec %4.4s", p_es->i_id, (const char*)&p_es->i_codec );
                        canRemux = false;
                    }
                    else if (i_codec_audio == 0)
                        i_codec_audio = p_es->i_codec;
                }
                else if (canDisplay==HAS_VIDEO && p_es->i_cat == VIDEO_ES)
                {
                    if (!canDecodeVideo( p_es ))
                    {
                        msg_Dbg( p_stream, "can't remux video track %d codec %4.4s", p_es->i_id, (const char*)&p_es->i_codec );
                        canRemux = false;
                    }
                    else if (i_codec_video == 0)
                        i_codec_video = p_es->i_codec;
                }
                else
                {
                    p_es->i_priority = ES_PRIORITY_NOT_SELECTABLE;
                    msg_Dbg( p_stream, "disable non audio/video track %d i_cat:%d codec %4.4s canDisplay:%d", p_es->i_id, p_es->i_cat, (const char*)&p_es->i_codec, canDisplay );
                }
            }
        }

        int i_port = var_InheritInteger(p_stream, CONTROL_CFG_PREFIX "http-port");

        if (deviceIP.empty())
            s_sout = "";
        else
        {
            std::stringstream ssout;
            ssout << '#';
            if ( !canRemux )
            {
                if ( i_codec_audio == 0 )
                    i_codec_audio = DEFAULT_TRANSCODE_AUDIO;
                /* avcodec AAC encoder is experimental */
                if ( i_codec_audio == VLC_CODEC_MP4A ||
                     i_codec_audio == VLC_FOURCC('h', 'a', 'a', 'c') ||
                     i_codec_audio == VLC_FOURCC('l', 'a', 'a', 'c') ||
                     i_codec_audio == VLC_FOURCC('s', 'a', 'a', 'c'))
                    i_codec_audio = DEFAULT_TRANSCODE_AUDIO;

                if ( i_codec_video == 0 )
                    i_codec_video = DEFAULT_TRANSCODE_VIDEO;

                /* TODO: provide audio samplerate and channels */
                ssout << "transcode{acodec=";
                char s_fourcc[5];
                vlc_fourcc_to_char( i_codec_audio, s_fourcc );
                s_fourcc[4] = '\0';
                ssout << s_fourcc;
                if ( canDisplay==HAS_VIDEO )
                {
                    /* TODO: provide maxwidth,maxheight */
                    ssout << ",vcodec=";
                    vlc_fourcc_to_char( i_codec_video, s_fourcc );
                    s_fourcc[4] = '\0';
                    ssout << s_fourcc;
                }
                ssout << "}:";
            }
            if (mime == "video/x-matroska" && canDisplay==HAS_VIDEO && i_codec_audio == VLC_CODEC_VORBIS && i_codec_video == VLC_CODEC_VP8 )
                mime == "video/webm";
            if (mime == "video/x-matroska" && !canDisplay==HAS_VIDEO )
                mime == "audio/x-matroska";
            ssout << "cc_sout{http-port=" << i_port << ",mux=" << muxer << ",mime=" << mime << ",uid=" << i_sout_id++ << "}";
            s_sout = ssout.str();
        }
        if (conn_status != CHROMECAST_CONNECTION_DEAD)
        {
            plugOutputRedirection();
        }
    }
    vlc_mutex_unlock(&lock);
}

void intf_sys_t::setCurrentStopped(bool stopped) {
    if (currentStopped != stopped)
    {
#ifndef NDEBUG
        msg_Dbg(p_stream, "change current file stopped, now %d", stopped);
#endif
        currentStopped = stopped;
        vlc_cond_broadcast(&loadCommandCond);
    }
}

void intf_sys_t::sendPlayerCmd()
{
    if (!p_input)
    {
        msg_Warn(p_stream, "no input");
        return;
    }

    assert(!p_input->b_preparsing);

    if (conn_status != CHROMECAST_APP_STARTED)
    {
        msg_Dbg(p_stream, "don't send playback command until the app is started");
        return;
    }

#ifndef NDEBUG
    msg_Dbg( p_stream, "sendPlayerCmd input_state:%d mediaSessionId:'%s' cmd_status:%d", (int)var_GetInteger( p_input, "state" ), mediaSessionId.c_str(), cmd_status );
#endif
    switch( var_GetInteger( p_input, "state" ) )
    {
    case OPENING_S:
        if (!mediaSessionId.empty()) {
            msg_Warn(p_stream, "opening when a session was still opened:%s", mediaSessionId.c_str());
#if 0
            msgPlayerStop();
#endif
            //mediaSessionId = "";
        }
        else
        //playback_start_chromecast = -1.0;
        if (cmd_status == NO_CMD_PENDING) {
            msgPlayerLoad();
            setPlayerStatus(CMD_LOAD_SENT);
        }
        break;
    case PLAYING_S:
        if (!mediaSessionId.empty() && receiverState != RECEIVER_IDLE && currentStopped) {
            msgPlayerPlay();
            setPlayerStatus(CMD_PLAYBACK_SENT);
        } else if (cmd_status == NO_CMD_PENDING) {
            msgPlayerLoad();
            setPlayerStatus(CMD_LOAD_SENT);
        }
        break;
    case PAUSE_S:
        if (!mediaSessionId.empty() && receiverState != RECEIVER_IDLE && currentStopped) {
            msgPlayerPause();
            setPlayerStatus(CMD_PLAYBACK_SENT);
        } else if (cmd_status == NO_CMD_PENDING) {
            msgPlayerLoad();
            setPlayerStatus(CMD_LOAD_SENT);
        }
        break;
    case END_S:
#if 0
        /* the MediaPlayer app doesn't like to be stopped, it won't restart after that */
        if (!mediaSessionId.empty() /* && receiverState == RECEIVER_BUFFERING */) {
            msgPlayerStop();

            /* TODO reset the sout as we'll need another one for the next load */
            //var_SetString( p_input, "sout", NULL );
            //mediaSessionId = ""; // it doesn't seem to send a status update like it should
            //setPlayerStatus(NO_CMD_PENDING); /* TODO: may not be needed */
        }
#endif
        break;
    default:
        //msgClose();
        break;
    }
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

    if( val.i_int == INPUT_EVENT_STATE )
    {
        msg_Info(p_this, "%ld playback state changed %d", GetCurrentThreadId(), (int)var_GetInteger( p_input, "state" ));
        vlc_mutex_locker locker(&p_sys->lock);
        p_sys->sendPlayerCmd();
    }

    return VLC_SUCCESS;
}

/**
 * @brief Connect to the Chromecast
 * @return the opened socket file descriptor or -1 on error
 */
int intf_sys_t::connectChromecast()
{
    int fd = net_ConnectTCP(p_stream, deviceIP.c_str(), devicePort);
    if (fd < 0)
        return -1;

    p_creds = vlc_tls_ClientCreate(VLC_OBJECT(p_stream));
    if (p_creds == NULL)
    {
        net_Close(fd);
        return -1;
    }

    p_tls = vlc_tls_ClientSessionCreate(p_creds, fd, deviceIP.c_str(),
                                               "tcps", NULL, NULL);

    if (p_tls == NULL)
    {
        vlc_tls_Delete(p_creds);
        return -1;
    }

    return fd;
}


/**
 * @brief Disconnect from the Chromecast
 */
void intf_sys_t::disconnectChromecast()
{
    if (p_tls)
    {
        vlc_tls_SessionDelete(p_tls);
        vlc_tls_Delete(p_creds);
        p_tls = NULL;
        setConnectionStatus(CHROMECAST_DISCONNECTED);
        setPlayerStatus(NO_CMD_PENDING);
        setCurrentStopped( true );
        receiverState = RECEIVER_IDLE;
    }
}


/**
 * @brief Receive a data packet from the Chromecast
 * @param p_stream the sout_stream_t structure
 * @param b_msgReceived returns true if a message has been entirely received else false
 * @param i_payloadSize returns the payload size of the message received
 * @return the number of bytes received of -1 on error
 */
// Use here only C linkage and POD types as this function is a cancelation point.
extern "C" int recvPacket(vlc_object_t *p_stream, bool &b_msgReceived,
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
                msg_Err(p_stream, "No PONG answer received from the Chromecast");
                return 0; // Connection died
            }
            (*pi_wait_retries)--;
        }
        else
        {
            /* now expect a pong */
            *pi_wait_delay = PONG_WAIT_TIME;
            *pi_wait_retries = PONG_WAIT_RETRIES;
            msg_Warn(p_stream, "No PING received from the Chromecast, sending a PING");
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
        msg_Err(p_stream, "Packet too long: droping its data");

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
void intf_sys_t::processMessage(const castchannel::CastMessage &msg)
{
    const std::string & namespace_ = msg.namespace_();

#ifndef NDEBUG
    msg_Dbg(p_stream,"processMessage: %s->%s %s", namespace_.c_str(), msg.destination_id().c_str(), msg.payload_utf8().c_str());
#endif

    if (namespace_ == NAMESPACE_DEVICEAUTH)
    {
        castchannel::DeviceAuthMessage authMessage;
        authMessage.ParseFromString(msg.payload_binary());

        if (authMessage.has_error())
        {
            msg_Err(p_stream, "Authentification error: %d", authMessage.error().error_type());
        }
        else if (!authMessage.has_response())
        {
            msg_Err(p_stream, "Authentification message has no response field");
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
            msg_Dbg(p_stream, "PING received from the Chromecast");
            msgPong();
        }
        else if (type == "PONG")
        {
            msg_Dbg(p_stream, "PONG received from the Chromecast");
        }
        else
        {
            msg_Warn(p_stream, "Heartbeat command not supported: %s", type.c_str());
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
                        /* now we can start the Chromecast playback */
                        sendPlayerCmd();
                }
            }
            else
            {
                switch(getConnectionStatus())
                {
                /* If the app is no longer present */
                case CHROMECAST_APP_STARTED:
                    msg_Warn(p_stream, "app is no longer present. closing");
                    msgReceiverClose(appTransportId);
                    setConnectionStatus(CHROMECAST_CONNECTION_DEAD);
                    break;

                case CHROMECAST_AUTHENTICATED:
                    msg_Dbg(p_stream, "Chromecast was running no app, launch media_app");
                    appTransportId = "";
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
            msg_Err(p_stream, "Failed to start the MediaPlayer: %s",
                    (const char *)reason);
        }
        else
        {
            msg_Warn(p_stream, "Receiver command not supported: %s",
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
            msg_Dbg(p_stream, "Player state: %s sessionId:%d",
                    status[0]["playerState"].operator const char *(),
                    (int)(json_int_t) status[0]["mediaSessionId"]);

            vlc_mutex_locker locker(&lock);
            receiver_state oldPlayerState = receiverState;
            std::string newPlayerState = status[0]["playerState"].operator const char *();

            if (newPlayerState == "IDLE") {
                receiverState = RECEIVER_IDLE;
                mediaSessionId = ""; // this session is not valid anymore
            }
            else if (newPlayerState == "PLAYING")
                receiverState = RECEIVER_PLAYING;
            else if (newPlayerState == "BUFFERING")
                receiverState = RECEIVER_BUFFERING;
            else if (newPlayerState == "PAUSED")
                receiverState = RECEIVER_PAUSED;
            else if (!newPlayerState.empty())
                msg_Warn(p_stream, "Unknown Chromecast state %s", newPlayerState.c_str());

            char session_id[32];
            if( snprintf( session_id, sizeof(session_id), "%" PRId64, (json_int_t) status[0]["mediaSessionId"] ) >= (int)sizeof(session_id) )
            {
                msg_Err( p_stream, "snprintf() truncated string for mediaSessionId" );
                session_id[sizeof(session_id) - 1] = '\0';
            }
            if (!mediaSessionId.empty() && session_id[0] && mediaSessionId != session_id) {
                msg_Warn(p_stream, "different mediaSessionId detected %s was %s", mediaSessionId.c_str(), this->mediaSessionId.c_str());
                //msgPlayerLoad();
            }

            mediaSessionId = session_id;

            if (receiverState != oldPlayerState)
            {
#ifndef NDEBUG
                msg_Dbg(p_stream, "change Chromecast player state from %d to %d", oldPlayerState, receiverState);
#endif
                switch( receiverState )
                {
                case RECEIVER_BUFFERING:
                    if ( double(status[0]["currentTime"]) == 0.0 )
                    {
                        receiverState = oldPlayerState;
                        msg_Dbg(p_stream, "Invalid buffering time, keep previous state %d", oldPlayerState);
                    }
                    else
                    {
                        if (!mediaSessionId.empty())
                        {
                            playlist_t *p_playlist = pl_Get( p_stream );
                            msgPlayerSetMute( var_GetBool( p_playlist, "mute") );
                            msgPlayerSetVolume( var_GetFloat( p_playlist, "volume") );
                        }
                    }
                    break;

                case RECEIVER_PLAYING:
                    /* TODO reset demux PCR ? */
                    setCurrentStopped( false );
                    setPlayerStatus(CMD_PLAYBACK_SENT);
                    break;

                case RECEIVER_PAUSED:
                    if (!mediaSessionId.empty())
                    {
                        playlist_t *p_playlist = pl_Get( p_stream );
                        msgPlayerSetMute( var_GetBool( p_playlist, "mute") );
                        msgPlayerSetVolume( var_GetFloat( p_playlist, "volume") );
                    }
                    break;

                case RECEIVER_IDLE:
                    setCurrentStopped( false );
                    /* fall through */
                default:
                    setPlayerStatus(NO_CMD_PENDING);
                    sendPlayerCmd();
                    break;
                }
            }

            if (cmd_status == CMD_LOAD_SENT)
                sendPlayerCmd();
        }
        else if (type == "LOAD_FAILED")
        {
            msg_Err(p_stream, "Media load failed");
            msgReceiverClose(appTransportId);
            vlc_mutex_locker locker(&lock);
            setConnectionStatus(CHROMECAST_CONNECTION_DEAD);
        }
        else if (type == "INVALID_REQUEST")
        {
            msg_Dbg(p_stream, "We sent an invalid request reason:%s", (*p_data)["reason"].operator const char *());
        }
        else
        {
            msg_Warn(p_stream, "Media command not supported: %s",
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
            msg_Warn(p_stream, "received close message");
            vlc_mutex_locker locker(&lock);
            setConnectionStatus(CHROMECAST_CONNECTION_DEAD);
        }
        else
        {
            msg_Warn(p_stream, "Connection command not supported: %s",
                    type.c_str());
        }
    }
    else
    {
        msg_Err(p_stream, "Unknown namespace: %s", msg.namespace_().c_str());
    }
}

/*****************************************************************************
 * Message preparation
 *****************************************************************************/
void intf_sys_t::msgAuth()
{
    castchannel::DeviceAuthMessage authMessage;
    authMessage.mutable_challenge();

    pushMessage( NAMESPACE_DEVICEAUTH, authMessage.SerializeAsString(),
                 DEFAULT_CHOMECAST_RECEIVER, castchannel::CastMessage_PayloadType_BINARY );
}


void intf_sys_t::msgPing()
{
    std::string s("{\"type\":\"PING\"}");
    pushMessage( NAMESPACE_HEARTBEAT, s );
}


void intf_sys_t::msgPong()
{
    std::string s("{\"type\":\"PONG\"}");
    pushMessage( NAMESPACE_HEARTBEAT, s );
}

void intf_sys_t::msgConnect(const std::string & destinationId)
{
    std::string s("{\"type\":\"CONNECT\"}");
    pushMessage( NAMESPACE_CONNECTION, s, destinationId );
}


void intf_sys_t::msgReceiverClose(std::string destinationId)
{
    std::string s("{\"type\":\"CLOSE\"}");
    pushMessage( NAMESPACE_CONNECTION, s, destinationId );
    if (appTransportId == destinationId)
        appTransportId = "";
    setConnectionStatus( deviceIP.empty() ? CHROMECAST_DISCONNECTED : CHROMECAST_TLS_CONNECTED );
}

void intf_sys_t::msgReceiverGetStatus()
{
    std::stringstream ss;
    ss << "{\"type\":\"GET_STATUS\","
       <<  "\"requestId\":" << i_receiver_requestId++ << "}";

    pushMessage( NAMESPACE_RECEIVER, ss.str() );
}

void intf_sys_t::msgReceiverLaunchApp()
{
    std::stringstream ss;
    ss << "{\"type\":\"LAUNCH\","
       <<  "\"appId\":\"" << APP_ID << "\","
       <<  "\"requestId\":" << i_receiver_requestId++ << "}";

    pushMessage( NAMESPACE_RECEIVER, ss.str() );
}


std::string intf_sys_t::GetMedia()
{
    std::stringstream ss;

    input_item_t * p_item = input_GetItem(p_input);
    if ( p_item )
    {
        char *psz_name = input_item_GetTitleFbName( p_item );
        ss << "\"metadata\":{"
           << " \"metadataType\":0"
           << ",\"title\":\"" << psz_name << "\"";

        char *psz_arturl = input_item_GetArtworkURL( p_item );
        if ( psz_arturl && !strncmp(psz_arturl, "http", 4))
            ss << ",\"images\":[\"" << psz_arturl << "\"]";
        free( psz_arturl );

        ss << "},";
        free( psz_name );

        std::stringstream chromecast_url;
        if ( canDoDirect && canRemux )
        {
            char *psz_uri = input_item_GetURI(p_item);
            chromecast_url << psz_uri;
            msg_Dbg( p_stream, "using direct URL: %s", psz_uri );
            free( psz_uri );
        }
        else
        {
            int i_port = var_InheritInteger(p_stream, CONTROL_CFG_PREFIX "http-port");
            chromecast_url << "http://" << serverIP << ":" << i_port << "/stream";
        }
        s_chromecast_url = chromecast_url.str();

        msg_Dbg(p_stream,"s_chromecast_url: %s", s_chromecast_url.c_str());
    }

    ss << "\"contentId\":\"" << s_chromecast_url << "\""
       << ",\"streamType\":\"LIVE\""
       << ",\"contentType\":\"" << mime << "\"";

    return ss.str();
}

void intf_sys_t::msgPlayerLoad()
{
    std::stringstream ss;
    ss << "{\"type\":\"LOAD\","
       <<  "\"media\":{" << GetMedia() << "},"
       <<  "\"autoplay\":\"false\","
       <<  "\"requestId\":" << i_requestId++
       << "}";

    pushMediaPlayerMessage( ss );
}

void intf_sys_t::msgPlayerPlay()
{
    assert(!mediaSessionId.empty());

    std::stringstream ss;
    ss << "{\"type\":\"PLAY\","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << i_requestId++
       << "}";

    pushMediaPlayerMessage( ss );
}

void intf_sys_t::msgPlayerPause()
{
    assert(!mediaSessionId.empty());

    std::stringstream ss;
    ss << "{\"type\":\"PAUSE\","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
       <<  "\"requestId\":" << i_requestId++
       << "}";

    pushMediaPlayerMessage( ss );
}

void intf_sys_t::msgPlayerSetVolume(float f_volume)
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

void intf_sys_t::msgPlayerSetMute(bool b_mute)
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

/**
 * @brief Send a message to the Chromecast
 * @param msg the CastMessage to send
 * @return the number of bytes sent or -1 on error
 */
int intf_sys_t::sendMessage(const castchannel::CastMessage &msg)
{
    uint32_t i_size = msg.ByteSize();
    uint32_t i_sizeNetwork = hton32(i_size);

    char *p_data = new(std::nothrow) char[PACKET_HEADER_LEN + i_size];
    if (p_data == NULL)
        return -1;

    memcpy(p_data, &i_sizeNetwork, PACKET_HEADER_LEN);
    msg.SerializeWithCachedSizesToArray((uint8_t *)(p_data + PACKET_HEADER_LEN));

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

/*****************************************************************************
 * Chromecast thread
 *****************************************************************************/
static void* ChromecastThread(void* p_data)
{
    int canc;
    intf_thread_t *p_stream = reinterpret_cast<intf_thread_t*>(p_data);
    intf_sys_t *p_sys = p_stream->p_sys;
    p_sys->setConnectionStatus( CHROMECAST_DISCONNECTED );

    p_sys->i_sock_fd = p_sys->connectChromecast();
    if (p_sys->i_sock_fd < 0)
    {
        canc = vlc_savecancel();
        msg_Err(p_stream, "Could not connect the Chromecast");
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
        msg_Err(p_stream, "Cannot get local IP address");
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

    /* HACK: determine if this is a Chromecast Audio */
    std::stringstream s_video_test;
    s_video_test << "http://" << p_sys->deviceIP << ":8008/apps/YouTube";
    access_t *p_test_app = vlc_access_NewMRL( VLC_OBJECT(p_stream), s_video_test.str().c_str() );
    p_sys->setCanDisplay( p_test_app != NULL ? HAS_VIDEO : AUDIO_ONLY );
    if ( p_test_app )
        vlc_access_Delete( p_test_app );

    vlc_restorecancel(canc);

    while (1)
    {
        p_sys->handleMessages();

        vlc_mutex_locker locker(&lock);
        if ( p_sys->getConnectionStatus() == CHROMECAST_CONNECTION_DEAD )
            break;
    }

    return NULL;
}

void intf_sys_t::handleMessages()
{
    unsigned i_received = 0;
    uint8_t p_packet[PACKET_MAX_LEN];
    bool b_pingTimeout = false;

    int i_waitdelay = PING_WAIT_TIME;
    int i_retries = PING_WAIT_RETRIES;

    bool b_msgReceived = false;
    uint32_t i_payloadSize = 0;
    int i_ret = recvPacket(VLC_OBJECT(p_stream), b_msgReceived, i_payloadSize, i_sock_fd,
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
        msg_Err(p_stream, "The connection to the Chromecast died (receiving).");
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

    // Send the answer messages if there is any.
    if (!messagesToSend.empty())
    {
        i_ret = sendMessages();
#if defined(_WIN32)
        if ((i_ret < 0 && WSAGetLastError() != WSAEWOULDBLOCK) || (i_ret == 0))
#else
        if ((i_ret < 0 && errno != EAGAIN) || i_ret == 0)
#endif
        {
            msg_Err(p_stream, "The connection to the Chromecast died (sending).");
            vlc_mutex_locker locker(&lock);
            setConnectionStatus(CHROMECAST_CONNECTION_DEAD);
        }
    }

    vlc_restorecancel(canc);
}

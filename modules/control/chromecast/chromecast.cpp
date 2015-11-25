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
#include <vlc_access.h>

#include <atomic>
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

#define DEFAULT_TRANSCODE_AUDIO   VLC_CODEC_MP3
#define DEFAULT_TRANSCODE_VIDEO   VLC_CODEC_H264

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
    PLAYER_SEEK_SENT,
};

enum player_state {
    STATE_IDLE,
    STATE_PLAYING,
    STATE_BUFFERING,
    STATE_PAUSED,
};

#define PACKET_MAX_LEN 10 * 1024
#define PACKET_HEADER_LEN 4

// Media player Chromecast app id
#define MEDIA_RECEIVER_APP_ID "CC1AD845" // Default media player aka DEFAULT_MEDIA_RECEIVER_APPLICATION_ID
static const std::string DEFAULT_CHOMECAST_RECEIVER = "receiver-0";
static const mtime_t SEEK_FORWARD_OFFSET = 1000000;

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
        ,playerState(STATE_IDLE)
        ,date_play_start(-1)
        ,playback_start_chromecast(-1.0)
        ,playback_start_local(0)
        ,canPause(false)
        ,canDisplay(false)
        ,i_sock_fd(-1)
        ,p_creds(NULL)
        ,p_tls(NULL)
        ,conn_status(CHROMECAST_DISCONNECTED)
        ,play_status(PLAYER_IDLE)
        ,i_supportedMediaCommands(15)
        ,m_seektime(-1.0)
        ,i_seektime(-1.0)
        ,i_app_requestId(0)
        ,i_requestId(0)
        ,i_sout_id(0)
    {
        p_interrupt = vlc_interrupt_create();
    }

    ~intf_sys_t()
    {
        disconnectChromecast();

#ifdef HAVE_MICRODNS
        mdns_cleanup(microdns_ctx);
#endif
        vlc_interrupt_destroy(p_interrupt);
    }

    mtime_t getPlaybackTime() const {
        switch( playerState )
        {
        case STATE_PLAYING:
            return ( mdate() - date_play_start ) + playback_start_local;

        case STATE_IDLE:
            msg_Dbg(p_intf, "receiver idle using buffering time %" PRId64, playback_start_local);
            break;
        case STATE_BUFFERING:
            msg_Dbg(p_intf, "receiver buffering using buffering time %" PRId64, playback_start_local);
            break;
        case STATE_PAUSED:
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

    void setCanDisplay(bool canDisplay) {
        vlc_mutex_locker locker(&lock);
        this->canDisplay = canDisplay;
    }

    bool isFinishedPlaying() {
        vlc_mutex_locker locker(&lock);
        return conn_status == CHROMECAST_DEAD || (playerState == STATE_BUFFERING && play_status != PLAYER_SEEK_SENT);
    }

    bool seekTo(mtime_t pos);

    void sendPlayerCmd();

    intf_thread_t  * const p_intf;
    input_thread_t *p_input;
    uint16_t       devicePort;
    std::string    deviceIP;
    std::string    serverIP;
    std::string    mime;
    std::string    muxer;
#ifdef HAVE_MICRODNS
    struct mdns_ctx *microdns_ctx;
    mtime_t         i_timeout;
    std::string     nameChromecast; /* name we're looking for */
#endif

    std::string appTransportId;
    std::string mediaSessionId;
    player_state playerState;

    mtime_t     date_play_start;
    mtime_t     playback_start_chromecast;
    mtime_t     playback_start_local;
    bool        canPause;
    bool        canDisplay;

    int i_sock_fd;
    vlc_tls_creds_t *p_creds;
    vlc_tls_t *p_tls;

    enum connection_status conn_status;
    enum player_status     play_status;
    int                    i_supportedMediaCommands;
    /* internal seek time */
    mtime_t                m_seektime;
    /* seek time with Chromecast relative timestamp */
    mtime_t                i_seektime;

    vlc_interrupt_t *p_interrupt;
    vlc_mutex_t  lock;
    vlc_cond_t   loadCommandCond;
    vlc_cond_t   seekCommandCond;
    vlc_thread_t chromecastThread;

    void msgAuth();
    void msgPing();
    void msgPong();
    void msgConnect(const std::string & destinationId = DEFAULT_CHOMECAST_RECEIVER);

    void msgReceiverLaunchApp();
    void msgReceiverClose();
    void msgReceiverGetStatus();
    void msgPlayerGetStatus();

    void InputUpdated( input_thread_t * );

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
    unsigned i_sout_id;

    std::string       s_sout;
    std::string       s_chromecast_url;
    bool              canRemux;
    bool              canDoDirect;

    void msgPlayerLoad();
    void msgPlayerStop();
    void msgPlayerPlay();
    void msgPlayerPause();
    void msgPlayerSeek(const std::string & currentTime);

    std::string GetMedia();

    bool canDecodeVideo( es_format_t * );
    bool canDecodeAudio( es_format_t * );

    void disconnectChromecast();
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
    add_string(CONTROL_CFG_PREFIX "muxer", "avformat{mux=matroska}", MUXER_TEXT, MUXER_LONGTEXT, false)
    set_callbacks( Open, Close )

    /* demuxer wrapper to report the playback time from the chromecast, rather than the
     * one from the underlying demuxer
     */
    add_submodule()
        set_shortname( "cc_demux" )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_DEMUX )
        set_description( N_( "chromecast demux wrapper" ) )
        set_capability( "demux", 0 )
        add_shortcut( "cc_demux" )
        set_callbacks( DemuxOpen, DemuxClose )

    /* sout wrapper that can tell when the Chromecast is finished playing
     * rather than when data are finished sending
     */
    add_submodule()
        set_shortname( "cc_sout" )
        set_category(CAT_SOUT)
        set_subcategory(SUBCAT_SOUT_STREAM)
        set_description( N_( "chromecast sout wrapper" ) )
        set_capability("sout stream", 0)
        add_shortcut("cc_sout")
        set_callbacks( SoutOpen, SoutClose )

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

    psz_mime = var_InheritString(p_intf, CONTROL_CFG_PREFIX "muxer");
    if (psz_mime == NULL)
    {
        msg_Err(p_intf, "Bad muxer provided");
        goto error;
    }
    p_sys->muxer = psz_mime; /* TODO get the MIME type from the playlist/input ? */
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

    assert( p_sys->p_input == NULL || p_sys->p_input == oldval.p_address );

    p_sys->InputUpdated( p_input );

    return VLC_SUCCESS;
}

bool intf_sys_t::canDecodeVideo( es_format_t *p_es )
{
    if (p_es->i_codec == VLC_CODEC_H264 || p_es->i_codec == VLC_CODEC_VP8)
        return true;
    return false;
}

bool intf_sys_t::canDecodeAudio( es_format_t *p_es )
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


void intf_sys_t::InputUpdated( input_thread_t *p_input )
{
    vlc_mutex_locker locker(&lock);
    assert(this->p_input != p_input);

    msg_Dbg(p_intf, "PlaylistEvent input changed");

    if( this->p_input != NULL )
    {
        var_DelCallback( this->p_input, "intf-event", InputEvent, p_intf );
        var_SetAddress( this->p_input->p_parent, SOUT_INTF_ADDRESS, NULL );
    }

    this->p_input = p_input;

    if( p_input != NULL )
    {
        var_AddCallback( p_input, "intf-event", InputEvent, p_intf );

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
                        msg_Dbg( p_intf, "can't remux audio track %d codec %4.4s", p_es->i_id, (const char*)&p_es->i_codec );
                        canRemux = false;
                    }
                    else if (i_codec_audio == 0)
                        i_codec_audio = p_es->i_codec;
                }
                else if (canDisplay && p_es->i_cat == VIDEO_ES)
                {
                    if (!canDecodeVideo( p_es ))
                    {
                        msg_Dbg( p_intf, "can't remux video track %d codec %4.4s", p_es->i_id, (const char*)&p_es->i_codec );
                        canRemux = false;
                    }
                    else if (i_codec_video == 0)
                        i_codec_video = p_es->i_codec;
                }
                else
                {
                    p_es->i_priority = ES_PRIORITY_NOT_SELECTABLE;
                    msg_Dbg( p_intf, "disable non audio/video track %d i_cat:%d codec %4.4s", p_es->i_id, p_es->i_cat, (const char*)&p_es->i_codec );
                }
            }
        }

        int i_port = var_InheritInteger(p_intf, CONTROL_CFG_PREFIX "http-port");

        std::stringstream ssout;
        ssout << "#";
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
            if ( canDisplay )
            {
                /* TODO: provide maxwidth,maxheight */
                ssout << ",vcodec=";
                vlc_fourcc_to_char( i_codec_video, s_fourcc );
                s_fourcc[4] = '\0';
                ssout << s_fourcc;
            }
            ssout << "}:";
        }
        if (mime == "video/x-matroska" && canDisplay && i_codec_audio == VLC_CODEC_VORBIS && i_codec_video == VLC_CODEC_VP8 )
            mime == "video/webm";
        if (mime == "video/x-matroska" && !canDisplay )
            mime == "audio/x-matroska";
        ssout << "cc_sout{http-port=" << i_port << ",mux=" << muxer << ",mime=" << mime << ",uid=" << i_sout_id++ << "}";
        s_sout = ssout.str();

        var_Create( p_input->p_parent, SOUT_INTF_ADDRESS, VLC_VAR_ADDRESS );
        var_SetAddress( p_input->p_parent, SOUT_INTF_ADDRESS, p_intf );

        var_SetString( p_input, "sout", s_sout.c_str() );
        var_SetString( p_input, "demux-filter", "cc_demux" );
    }
}

void intf_sys_t::sendPlayerCmd()
{
    if (conn_status != CHROMECAST_APP_STARTED)
    {
        msg_Dbg(p_intf, "don't send playback command until the app is started");
        return;
    }

    if (!p_input)
    {
        msg_Warn(p_intf, "no input");
        return;
    }

    assert(!p_input->b_preparsing);

    switch( var_GetInteger( p_input, "state" ) )
    {
    case OPENING_S:
        if (!mediaSessionId.empty()) {
            msg_Dbg(p_intf, "opening when a session was still opened:%s", mediaSessionId.c_str());
            msgPlayerStop();
            mediaSessionId = "";
        }
        //playback_start_chromecast = -1.0;
        playback_start_local = 0;
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
            //var_SetString( p_input, "sout", NULL );
            mediaSessionId = ""; // it doesn't seem to send a status update like it should
            play_status = PLAYER_IDLE; /* TODO: may not be needed */
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

    assert(playback_start_chromecast != -1.0);

    i_seektime = mdate() + SEEK_FORWARD_OFFSET /* + playback_start_local */ ;
    const std::string currentTime = std::to_string( double( i_seektime ) / 1000000.0 );
    m_seektime = pos; // + playback_start_chromecast - playback_start_local;
    playback_start_local = pos;
    msg_Dbg( p_intf, "%ld Seeking to %" PRId64 "/%s playback_time:%" PRId64, GetCurrentThreadId(), pos, currentTime.c_str(), playback_start_chromecast);
    play_status = PLAYER_SEEK_SENT;
    msgPlayerSeek( currentTime );

    return true;
}

static const char *event_names[] = {
    "INPUT_EVENT_STATE",
    /* b_dead is true */
    "INPUT_EVENT_DEAD",

    /* "rate" has changed */
    "INPUT_EVENT_RATE",

    /* At least one of "position" or "time" */
    "INPUT_EVENT_POSITION",

    /* "length" has changed */
    "INPUT_EVENT_LENGTH",

    /* A title has been added or removed or selected.
     * It imply that chapter has changed (not chapter event is sent) */
    "INPUT_EVENT_TITLE",
    /* A chapter has been added or removed or selected. */
    "INPUT_EVENT_CHAPTER",

    /* A program ("program") has been added or removed or selected",
     * or "program-scrambled" has changed.*/
    "INPUT_EVENT_PROGRAM",
    /* A ES has been added or removed or selected */
    "INPUT_EVENT_ES",
    /* "teletext-es" has changed */
    "INPUT_EVENT_TELETEXT",

    /* "record" has changed */
    "INPUT_EVENT_RECORD",

    /* "INPUT_item_t media has changed */
    "INPUT_EVENT_ITEM_META",
    /* "INPUT_item_t info has changed */
    "INPUT_EVENT_ITEM_INFO",
    /* "INPUT_item_t name has changed */
    "INPUT_EVENT_ITEM_NAME",
    /* "INPUT_item_t epg has changed */
    "INPUT_EVENT_ITEM_EPG",

    /* Input statistics have been updated */
    "INPUT_EVENT_STATISTICS",
    /* At least one of "signal-quality" or "signal-strength" has changed */
    "INPUT_EVENT_SIGNAL",

    /* "audio-delay" has changed */
    "INPUT_EVENT_AUDIO_DELAY",
    /* "spu-delay" has changed */
    "INPUT_EVENT_SUBTITLE_DELAY",

    /* "bookmark" has changed */
    "INPUT_EVENT_BOOKMARK",

    /* cache" has changed */
    "INPUT_EVENT_CACHE",

    /* A audio_output_t object has been created/deleted by *the input* */
    "INPUT_EVENT_AOUT",
    /* A vout_thread_t object has been created/deleted by *the input* */
    "INPUT_EVENT_VOUT",
};

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
    case INPUT_EVENT_POSITION:
        msg_Info(p_this, "position changed event %f/%" PRId64, var_GetFloat( p_input, "position" ), var_GetInteger( p_input, "time" ));
        return VLC_SUCCESS;

    case INPUT_EVENT_STATISTICS:
    case INPUT_EVENT_CACHE:
        /* discard logs */
        return VLC_SUCCESS;
    }

    msg_Dbg(p_this, "InputEvent %s", event_names[val.i_int]);

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
void intf_sys_t::disconnectChromecast()
{
    if (p_tls)
    {
        vlc_tls_SessionDelete(p_tls);
        vlc_tls_Delete(p_creds);
        p_tls = NULL;
        conn_status = CHROMECAST_DISCONNECTED;
    }
}

/**
 * @brief Send a message to the Chromecast
 * @param msg the CastMessage to send
 * @return vlc error code
 */
int intf_sys_t::sendMessage(castchannel::CastMessage &msg)
{
    uint32_t i_size = msg.ByteSize();
    uint8_t *p_data = new(std::nothrow) uint8_t[PACKET_HEADER_LEN + i_size];
    if (p_data == NULL)
        return VLC_ENOMEM;

#ifndef NDEBUG
    msg_Dbg(p_intf, "sendMessage: %s->%s %s", msg.namespace_().c_str(), msg.destination_id().c_str(), msg.payload_utf8().c_str());
#endif

    SetDWBE(p_data, i_size);
    msg.SerializeWithCachedSizesToArray(p_data + PACKET_HEADER_LEN);

    int i_ret = tls_Send(p_tls, p_data, PACKET_HEADER_LEN + i_size);
    delete[] p_data;
    if (i_ret == PACKET_HEADER_LEN + i_size)
        return VLC_SUCCESS;

    return VLC_EGENERIC;
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
    msg_Dbg(p_intf,"processMessage: %s->%s %s", namespace_.c_str(), msg.destination_id().c_str(), msg.payload_utf8().c_str());
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
            msg_Warn(p_intf, "Heartbeat command not supported: %s", type.c_str());
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
                    //msg_Dbg(p_intf, "Chromecast was running app:%s, launch media_app", appId.c_str());
                    //p_sys->appTransportId = "";
                    //p_sys->msgReceiverLaunchApp();
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
                else
                {
                    p_sys->msgPlayerGetStatus();
                    //p_sys->msgReceiverGetStatus();
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
                    break;

                case CHROMECAST_AUTHENTICATED:
                    msg_Dbg(p_intf, "Chromecast was running no app, launch media_app");
                    p_sys->appTransportId = "";
                    p_sys->msgReceiverLaunchApp();
                    break;

                default:
                    break;
                }

            }
        }
        else if (type == "LAUNCH_ERROR")
        {
            json_value reason = (*p_data)["reason"];
            msg_Err(p_intf, "Failed to start the MediaPlayer: %s",
                    (const char *)reason);
            i_ret = -1;
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
            p_sys->i_supportedMediaCommands = status[0]["supportedMediaCommands"].operator json_int_t();

            player_state oldPlayerState = p_sys->playerState;
            std::string newPlayerState = status[0]["playerState"].operator const char *();

            if (newPlayerState == "IDLE")
                p_sys->playerState = STATE_IDLE;
            else if (newPlayerState == "PLAYING")
                p_sys->playerState = STATE_PLAYING;
            else if (newPlayerState == "BUFFERING")
                p_sys->playerState = STATE_BUFFERING;
            else if (newPlayerState == "PAUSED")
                p_sys->playerState = STATE_PAUSED;
            else
                msg_Warn(p_intf, "Unknown Chromecast state %s", newPlayerState.c_str());

            std::string mediaSessionId = std::to_string((json_int_t) status[0]["mediaSessionId"]);
            if (!mediaSessionId.empty() && !p_sys->mediaSessionId.empty() && mediaSessionId != p_sys->mediaSessionId ) {
                msg_Warn(p_intf, "different mediaSessionId detected %s was %s", mediaSessionId.c_str(), p_sys->mediaSessionId.c_str());
                //p_sys->msgPlayerLoad();
            }

            p_sys->mediaSessionId = mediaSessionId;

            if (p_sys->playerState != oldPlayerState)
            {
                switch( p_sys->playerState )
                {
                case STATE_BUFFERING:
                    p_sys->playback_start_chromecast = mtime_t( double( status[0]["currentTime"] ) * 1000000.0 );
                    msg_Dbg(p_intf, "Playback pending with an offset of %" PRId64, p_sys->playback_start_chromecast);
                    break;

                case STATE_PLAYING:
                    /* TODO reset demux PCR ? */
                    if (unlikely(p_sys->playback_start_chromecast == -1.0)) {
                        msg_Warn(p_intf, "start playing without buffering");
                        p_sys->playback_start_chromecast = mtime_t( double( status[0]["currentTime"] ) * 1000000.0 );
                    }
                    p_sys->play_status = PLAYER_PLAYBACK_SENT;
                    p_sys->date_play_start = mdate();
                    msg_Dbg(p_intf, "Playback started with an offset of %" PRId64, p_sys->playback_start_chromecast);
                    break;

                case STATE_PAUSED:
                    p_sys->playback_start_local += mdate() - p_sys->date_play_start;
                    break;

                default:
                    p_sys->play_status = PLAYER_IDLE;
                    p_sys->date_play_start = -1;
                    break;
                }
            }

            if (p_sys->playerState == STATE_BUFFERING && p_sys->i_seektime != -1.0)
            {
                msg_Dbg(p_intf, "Chromecast seeking possibly done");
                vlc_cond_signal( &p_sys->seekCommandCond );
            }

            if (p_sys->play_status == PLAYER_LOAD_SENT)
                p_sys->sendPlayerCmd();
        }
        else if (type == "LOAD_FAILED")
        {
            msg_Err(p_intf, "Media load failed");
            vlc_mutex_locker locker(&p_sys->lock);
            p_sys->msgReceiverClose();
        }
        else if (type == "INVALID_REQUEST")
        {
            msg_Dbg(p_intf, "We sent an invalid request reason:%s", (*p_data)["reason"].operator const char *());
        }
        else
        {
            msg_Warn(p_intf, "Media command not supported: %s",
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
            vlc_cond_signal(&p_sys->seekCommandCond);
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

    vlc_mutex_locker locker(&lock);
    sendMessage(msg);
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
            msg_Dbg( p_intf, "using direct URL: %s", psz_uri );
            free( psz_uri );
        }
        else
        {
            int i_port = var_InheritInteger(p_intf, CONTROL_CFG_PREFIX "http-port");
            chromecast_url << "http://" << serverIP << ":" << i_port << "/stream";
        }
        s_chromecast_url = chromecast_url.str();

        msg_Dbg(p_intf,"s_chromecast_url: %s", s_chromecast_url.c_str());
    }

    ss << "\"contentId\":\"" << s_chromecast_url << "\""
       << ",\"streamType\":\"LIVE\""
       << ",\"contentType\":\"" << mime << "\"";

    return ss.str();
}

void intf_sys_t::msgPlayerGetStatus()
{
    std::stringstream ss;
    ss << "{\"type\":\"GET_STATUS\","
       <<  "\"requestId\":" << i_requestId++
       << "}";

    pushMediaPlayerMessage( ss );
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

void intf_sys_t::msgPlayerStop()
{
    assert(!mediaSessionId.empty());

    std::stringstream ss;
    ss << "{\"type\":\"STOP\","
       <<  "\"mediaSessionId\":" << mediaSessionId << ","
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

void intf_sys_t::msgPlayerSeek(const std::string & currentTime)
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

    /* HACK: determine if this is a Chromecast Audio */
    std::stringstream s_video_test;
    s_video_test << "http://" << p_sys->deviceIP << ":8008/apps/YouTube";
    access_t *p_test_app = vlc_access_NewMRL( VLC_OBJECT(p_intf), s_video_test.str().c_str() );
    p_sys->setCanDisplay( p_test_app != NULL );
    if ( p_test_app )
        vlc_access_Delete( p_test_app );

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
        return p_intf->p_sys->seekTo(i_pos);
    }

    void setLength(mtime_t length) {
        this->i_length = length;
    }

    int Demux() {
        vlc_mutex_lock(&p_intf->p_sys->lock);
        if (!demuxReady)
        {
            mutex_cleanup_push(&p_intf->p_sys->lock);

            while (p_intf->p_sys->conn_status != CHROMECAST_APP_STARTED &&
                   p_intf->p_sys->conn_status != CHROMECAST_DEAD)
                vlc_cond_wait(&p_intf->p_sys->loadCommandCond, &p_intf->p_sys->lock);

            vlc_cleanup_pop();

            demuxReady = true;
            msg_Dbg(p_demux, "ready to demux");
        }

        if (p_intf->p_sys->conn_status != CHROMECAST_APP_STARTED) {
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

            while (p_intf->p_sys->playback_start_chromecast < p_intf->p_sys->i_seektime)
            {
                msg_Dbg(p_demux, "%ld waiting for Chromecast seek", GetCurrentThreadId());
                vlc_cond_wait(&p_intf->p_sys->seekCommandCond, &p_intf->p_sys->lock);
                msg_Dbg(p_demux, "%ld finished waiting for Chromecast seek", GetCurrentThreadId());
            }
            p_intf->p_sys->m_seektime = -1.0;
            p_intf->p_sys->i_seektime = -1.0;

            if (p_intf->p_sys->conn_status != CHROMECAST_APP_STARTED) {
                msg_Warn(p_demux, "cannot seek as the Chromecast app is not running %d", p_intf->p_sys->conn_status);
                vlc_mutex_unlock(&p_intf->p_sys->lock);
                return 0;
            }
        }
        vlc_mutex_unlock(&p_intf->p_sys->lock);

        return p_demux->p_source->pf_demux( p_demux->p_source );
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

struct sout_stream_sys_t
{
    sout_stream_sys_t(sout_stream_t *stream, intf_thread_t *intf, sout_stream_t *sout)
        :p_wrapped(sout)
        ,p_intf(intf)
        ,p_stream(stream)
        ,b_header_started(false)
    {
        assert(p_intf != NULL);
        vlc_object_hold(p_intf);
    }

    ~sout_stream_sys_t()
    {
        sout_StreamChainDelete(p_wrapped, p_wrapped);
        vlc_object_release(p_intf);
    }

    bool isFinishedPlaying() const {
        /* check if the Chromecast to be done playing */
        return p_intf->p_sys->isFinishedPlaying();
    }

    sout_stream_t * const p_wrapped;

protected:
    intf_thread_t * const p_intf;
    sout_stream_t * const p_stream;
    bool                  b_header_started;
};

/*****************************************************************************
 * Sout callbacks
 *****************************************************************************/
static sout_stream_id_sys_t *Add(sout_stream_t *p_stream, const es_format_t *p_fmt)
{
    return sout_StreamIdAdd( p_stream->p_sys->p_wrapped, p_fmt );
}

static void Del(sout_stream_t *p_stream, sout_stream_id_sys_t *id)
{
    sout_StreamIdDel( p_stream->p_sys->p_wrapped, id );
}

static int Send(sout_stream_t *p_stream, sout_stream_id_sys_t *id, block_t *p_buffer)
{
    return sout_StreamIdSend( p_stream->p_sys->p_wrapped, id, p_buffer );
}

static int Flush( sout_stream_t *p_stream, sout_stream_id_sys_t *id )
{
    return sout_StreamFlush( p_stream->p_sys->p_wrapped, id );
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

    return p_sys->p_wrapped->pf_control( p_sys->p_wrapped, i_query, args );
}

int SoutOpen(vlc_object_t *p_this)
{
    sout_stream_t *p_stream = reinterpret_cast<sout_stream_t*>(p_this);
    char *psz_var_mux = NULL, *psz_var_mime = NULL;
    intf_thread_t *p_intf = NULL;
    sout_stream_sys_t *p_sys = NULL;
    sout_stream_t *p_sout = NULL;
    std::stringstream ss;

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

    ss << "http{dst=:" << var_InheritInteger(p_stream, SOUT_CFG_PREFIX "http-port") << "/stream"
       << ",mux=" << psz_var_mux
       << ",access=http{mime=" << psz_var_mime << "}}";

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
    p_stream->pf_flush   = Flush;
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

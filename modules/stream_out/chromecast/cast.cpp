/*****************************************************************************
 * cast.cpp: Chromecast sout module for vlc
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
#include <vlc_sout.h>
#include <vlc_renderer.h>

#include <cassert>
#include <vector>

struct sout_stream_sys_t
{
    sout_stream_sys_t(vlc_renderer * const renderer, sout_stream_t *sout, bool has_video, int port,
                      const char *psz_default_muxer, const char *psz_default_mime)
        : p_out(sout)
        , default_muxer(psz_default_muxer)
        , default_mime(psz_default_mime)
        , p_renderer(renderer)
        , b_has_video(has_video)
        , i_port(port)
        , last_added_ts( VLC_TS_INVALID )
    {
    }
    
    ~sout_stream_sys_t()
    {
        sout_StreamChainDelete(p_out, p_out);
        vlc_object_release( p_renderer );
    }

    bool isFinishedPlaying() const {
        /* check if the Chromecast to be done playing */
        return p_renderer == NULL || p_renderer->p_sys->isFinishedPlaying();
    }

    bool canDecodeVideo( const es_format_t *p_es ) const;
    bool canDecodeAudio( const es_format_t *p_es ) const;

    sout_stream_t     *p_out;
    std::string        sout;
    const std::string  default_muxer;
    const std::string  default_mime;

    vlc_renderer * const p_renderer;
    const bool b_has_video;
    const int i_port;

    mtime_t                            last_added_ts;
    std::vector<es_format_t *>         outputs;
    std::vector<sout_stream_id_sys_t*> streams;
};

#define SOUT_CFG_PREFIX "sout-chromecast-"
const static mtime_t MAX_WAIT_BETWEEN_ADD = (CLOCK_FREQ / 3);

static const vlc_fourcc_t DEFAULT_TRANSCODE_AUDIO = VLC_CODEC_MP3;
static const vlc_fourcc_t DEFAULT_TRANSCODE_VIDEO = VLC_CODEC_H264;


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

static const char *const ppsz_sout_options[] = {
    "ip", "http-port", "mux", "mime", "video", NULL
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define HTTP_PORT_TEXT N_("HTTP port")
#define HTTP_PORT_LONGTEXT N_("This sets the HTTP port of the server " \
                              "used to stream the media to the Chromecast.")
#define HAS_VIDEO_TEXT N_("Video")
#define HAS_VIDEO_LONGTEXT N_("The Chromecast receiver can receive video.")
#define MUX_TEXT N_("Muxer")
#define MUX_LONGTEXT N_("This sets the muxer used to stream to the Chromecast.")
#define MIME_TEXT N_("MIME content type")
#define MIME_LONGTEXT N_("This sets the media MIME content type sent to the Chromecast.")
#define IP_ADDR_TEXT N_("IP Address")
#define IP_ADDR_LONGTEXT N_("IP Address of the Chromecast.")

vlc_module_begin ()

    set_shortname( "cc_sout" )
    set_description(N_("Chromecast stream output"))
    set_capability("sout stream", 0)
    add_shortcut("cc_sout")
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_STREAM)
    set_callbacks(Open, Close)

    add_string(SOUT_CFG_PREFIX "ip", NULL, IP_ADDR_TEXT, IP_ADDR_LONGTEXT, false)
    add_integer(SOUT_CFG_PREFIX "http-port", HTTP_PORT, HTTP_PORT_TEXT, HTTP_PORT_LONGTEXT, false)
    add_bool(SOUT_CFG_PREFIX "video", true, HAS_VIDEO_TEXT, HAS_VIDEO_LONGTEXT, false)
    add_string(SOUT_CFG_PREFIX "mux", "avformat{mux=matroska}", MUX_TEXT, MUX_LONGTEXT, false)
    add_string(SOUT_CFG_PREFIX "mime", "video/x-matroska", MIME_TEXT, MIME_LONGTEXT, false)

vlc_module_end ()


/*****************************************************************************
 * Sout callbacks
 *****************************************************************************/
static sout_stream_id_sys_t *Add(sout_stream_t *p_stream, const es_format_t *p_fmt)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if (!p_sys->b_has_video)
    {
        if (p_fmt->i_cat != AUDIO_ES)
            return NULL;
    }
    sout_stream_id_sys_t *result = sout_StreamIdAdd(p_sys->p_out, p_fmt);
    if ( result != NULL )
    {
        es_format_t *p_es = (es_format_t *)malloc( sizeof(es_format_t) );
        if (p_es != NULL)
        {
            es_format_Copy( p_es, p_fmt );
            p_sys->outputs.push_back( p_es );
            p_sys->streams.push_back( result );
            p_sys->last_added_ts = mdate();
        }
    }
    return result;
}


static void Del(sout_stream_t *p_stream, sout_stream_id_sys_t *id)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    for (size_t i=0; i<p_sys->streams.size(); i++)
    {
        if ( p_sys->streams[i] == id )
        {
            p_sys->streams.erase( p_sys->streams.begin() +  i );
            es_format_Clean( p_sys->outputs[i] );
            free( p_sys->outputs[i] );
            p_sys->outputs.erase( p_sys->outputs.begin() +  i );
            p_sys->last_added_ts = mdate();
            break;
        }
    }

    sout_StreamIdDel(p_sys->p_out, id);
}


bool sout_stream_sys_t::canDecodeVideo( const es_format_t *p_es ) const
{
    if (p_es->i_codec == VLC_CODEC_H264 || p_es->i_codec == VLC_CODEC_VP8)
        return true;
    return false;
}

bool sout_stream_sys_t::canDecodeAudio( const es_format_t *p_es ) const
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

static int Send(sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                block_t *p_buffer)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if ( p_sys->last_added_ts != VLC_TS_INVALID )
    {
        if ( p_sys->last_added_ts + MAX_WAIT_BETWEEN_ADD < mdate() )
        {
            // wait for new ES expired
            msleep( MAX_WAIT_BETWEEN_ADD );
        }
        p_sys->last_added_ts = VLC_TS_INVALID;

        bool canRemux = true;
        vlc_fourcc_t i_codec_video = 0, i_codec_audio = 0;

        for (std::vector<es_format_t*>::iterator it = p_sys->outputs.begin(); it != p_sys->outputs.begin(); ++it)
        {
            const es_format_t *p_es = *it;
            if (p_es->i_cat == AUDIO_ES)
            {
                if (!p_sys->canDecodeAudio( p_es ))
                {
                    msg_Dbg( p_stream, "can't remux audio track %d codec %4.4s", p_es->i_id, (const char*)&p_es->i_codec );
                    canRemux = false;
                }
                else if (i_codec_audio == 0)
                    i_codec_audio = p_es->i_codec;
            }
            else if (p_sys->b_has_video && p_es->i_cat == VIDEO_ES)
            {
                if (!p_sys->canDecodeVideo( p_es ))
                {
                    msg_Dbg( p_stream, "can't remux video track %d codec %4.4s", p_es->i_id, (const char*)&p_es->i_codec );
                    canRemux = false;
                }
                else if (i_codec_video == 0)
                    i_codec_video = p_es->i_codec;
            }
        }

        std::stringstream ssout;
        //ssout << '#';
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
            if ( p_sys->b_has_video )
            {
                /* TODO: provide maxwidth,maxheight */
                ssout << ",vcodec=";
                vlc_fourcc_to_char( i_codec_video, s_fourcc );
                s_fourcc[4] = '\0';
                ssout << s_fourcc;
            }
            ssout << "}:";
        }
        std::string mime;
        if ( !p_sys->b_has_video )
            mime = "audio/x-matroska";
        else if (i_codec_audio == VLC_CODEC_VORBIS && i_codec_video == VLC_CODEC_VP8 )
            mime = "video/webm";
        else
            mime = p_sys->default_mime;

        ssout << "http{dst=:" << p_sys->i_port << "/stream"
              << ",mux=" << p_sys->default_muxer
              << ",access=http{mime=" << mime << "}}";

        if ( p_sys->sout != ssout.str() )
        {
            sout_StreamChainDelete( p_sys->p_out, p_sys->p_out );
            p_sys->sout = "";

            p_sys->p_out = sout_StreamChainNew( p_stream->p_sout, ssout.str().c_str(), NULL, NULL);
            if (p_sys->p_out == NULL) {
                msg_Dbg(p_stream, "could not create sout chain:%s", ssout.str().c_str());
                return VLC_EGENERIC;
            }
            p_sys->sout = ssout.str();
        }

        /* FIXME tell the chromecast to load the content */
    }

    return sout_StreamIdSend(p_sys->p_out, id, p_buffer);
}

static void Flush( sout_stream_t *p_stream, sout_stream_id_sys_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    sout_StreamFlush( p_sys->p_out, id );
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

    if ( !p_sys->p_out->pf_control )
        return VLC_EGENERIC;

    return p_sys->p_out->pf_control( p_sys->p_out, i_query, args );
}

/*****************************************************************************
 * Open: connect to the Chromecast and initialize the sout
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    sout_stream_t *p_stream = reinterpret_cast<sout_stream_t*>(p_this);
    sout_stream_sys_t *p_sys = NULL;
    vlc_renderer *p_renderer = NULL;
    char *psz_ip = NULL;
    char *psz_mux = NULL;
    char *psz_var_mime = NULL;
    sout_stream_t *p_sout = NULL;
    bool b_has_video = true;
    int i_port;
    std::stringstream srender, ss;

    config_ChainParse(p_stream, SOUT_CFG_PREFIX, ppsz_sout_options, p_stream->p_cfg);

    psz_ip = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "ip");
    if ( psz_ip == NULL )
    {
        msg_Err( p_this, "missing Chromecast IP address" );
        goto error;
    }

    srender << "chromecast://" << psz_ip;
    p_renderer = vlc_renderer_new( p_this,  srender.str().c_str() ); /* FIXME load the class directly */
    if ( p_renderer == NULL)
    {
        msg_Err( p_this, "cannot load the Chromecast controler" );
        goto error;
    }

    psz_mux = var_GetNonEmptyString(p_stream, SOUT_CFG_PREFIX "mux");
    if (psz_mux == NULL)
    {
        goto error;
    }
    psz_var_mime = var_GetNonEmptyString(p_stream, SOUT_CFG_PREFIX "mime");
    if (psz_var_mime == NULL)
        goto error;

    i_port = var_InheritInteger(p_stream, SOUT_CFG_PREFIX "http-port");
    ss << "http{dst=:" << i_port << "/stream"
       << ",mux=" << psz_mux
       << ",access=http{mime=" << psz_var_mime << "}}";

    p_sout = sout_StreamChainNew( p_stream->p_sout, ss.str().c_str(), NULL, NULL);
    if (p_sout == NULL) {
        msg_Dbg(p_stream, "could not create sout chain:%s", ss.str().c_str());
        goto error;
    }

    b_has_video = var_GetBool(p_stream, SOUT_CFG_PREFIX "video");

    p_sys = new(std::nothrow) sout_stream_sys_t( p_renderer, p_sout, b_has_video, i_port,
                                                 psz_mux, psz_var_mime );
    if (unlikely(p_sys == NULL))
    {
        goto error;
    }
    p_sys->sout = ss.str();

    // Set the sout callbacks.
    p_stream->pf_add     = Add;
    p_stream->pf_del     = Del;
    p_stream->pf_send    = Send;
    p_stream->pf_flush   = Flush;
    p_stream->pf_control = Control;

    p_stream->p_sys = p_sys;
    free(psz_ip);
    free(psz_mux);
    free(psz_var_mime);
    return VLC_SUCCESS;

error:
    if ( p_renderer != NULL )
        vlc_object_release( p_renderer );
    sout_StreamChainDelete(p_sout, p_sout);
    free(psz_ip);
    free(psz_mux);
    free(psz_var_mime);
    delete p_sys;
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    sout_stream_t *p_stream = reinterpret_cast<sout_stream_t*>(p_this);

    delete p_stream->p_sys;
}


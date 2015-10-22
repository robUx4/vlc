/*****************************************************************************
 * simplehttpd.c: Simple HTTP Stream output
 *****************************************************************************
 * Copyright © 2015 VLC authors and VideoLAN
 * Copyright © 2015 by Steve Lhomme
 *
 * Authors: Steve Lhomme <robux4 at videolabs dot io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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

#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sstream>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_network.h>
#include <vlc_block.h>
#include <vlc_fs.h>
#include <vlc_strings.h>
#include <vlc_charset.h>
#include <vlc_mime.h>
#include <vlc_httpd.h> /* TODO remove */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-simplehttpd-"
#define RATECONTROL_TEXT N_("Use muxers rate control mechanism")

#define MIME_TEXT N_("Mime")
#define MIME_LONGTEXT N_("MIME returned by the server (autodetected " \
                        "if not specified)." )

vlc_module_begin ()
    set_description( N_("HTTP stream output") )
    set_shortname( N_("SimpleHTTP" ))
    add_shortcut( "simplehttpd" )
    set_capability( "sout access", 0 )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_ACO )
    add_string( SOUT_CFG_PREFIX "mime", "",
                MIME_TEXT, MIME_LONGTEXT, true )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "mime",
    NULL
};

static ssize_t Write( sout_access_out_t *, block_t * );
static int Seek ( sout_access_out_t *, off_t  );
static int Control( sout_access_out_t *, int, va_list );
static void* httpd_HostThread(void *);

enum http_state {
    STATE_LISTEN,
    STATE_READ_METHOD,
    STATE_READ_HEADERS,
    STATE_SEND_HEADER,
    STATE_SEND_BODY,
    STATE_DEAD,
};

struct sout_access_out_sys_t
{
    sout_access_out_sys_t()
        :pi_listen_fd(NULL)
        ,i_socket(-1)
        ,state(STATE_LISTEN)
        ,i_version(-1)
        ,i_status(500)
        ,status_text("Server Error")
    {}

    int             *pi_listen_fd;
    int             i_socket;
    vlc_thread_t    thread;
    enum http_state  state;

    std::string     mime;
    std::string     http_url;
    int i_version;
    int i_status;
    std::string status_text;
};

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t     *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys;

    config_ChainParse( p_access, SOUT_CFG_PREFIX, ppsz_sout_options, p_access->p_cfg );

    char *hostname = var_InheritString(p_this, SOUT_CFG_PREFIX "host");
    int bind_port = var_InheritInteger(p_this, SOUT_CFG_PREFIX "port");

    const char *path = p_access->psz_path;
    path += strcspn( path, "/" );
    if( path > p_access->psz_path )
    {
        const char *port = strrchr( p_access->psz_path, ':' );
        if( port != NULL && strchr( port, ']' ) != NULL )
            port = NULL; /* IPv6 numeral */
        if( port != p_access->psz_path )
        {
            int len = (port ? port : path) - p_access->psz_path;
            msg_Warn( p_access, "\"%.*s\" HTTP host might be ignored in "
                      "multiple-host configurations, use at your own risks.",
                      len, p_access->psz_path );
            msg_Info( p_access, "Consider passing --simplehttpd-host=IP on the "
                                "command line instead." );

            char host[len + 1];
            strncpy( host, p_access->psz_path, len );
            host[len] = '\0';

            var_Create( p_access, "host", VLC_VAR_STRING );
            var_SetString( p_access, "host", host );
        }
        if( port != NULL )
        {
            port++;

            bind_port = atoi( port );
        }
    }
    if( !*path )
        path = "/";

    p_sys = new(std::nothrow) sout_access_out_sys_t;
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    char *psz_mime = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "mime" );
    if (psz_mime != NULL)
    {
        p_sys->mime = psz_mime;
        free(psz_mime);
    }
    if (p_sys->mime.empty())
        p_sys->mime = vlc_mime_Ext2Mime(path);

    p_sys->pi_listen_fd = net_ListenTCP(p_access, hostname, bind_port);
    if (!p_sys->pi_listen_fd) {
        msg_Err(p_access, "cannot create socket(s) for HTTP host");
        free( p_sys );
        return VLC_ENOMEM;
    }

    p_sys->i_socket = -1;
    p_sys->state = STATE_LISTEN;

    /* start the server thread */
    if (vlc_clone(&p_sys->thread, httpd_HostThread, p_this,
                   VLC_THREAD_PRIORITY_LOW)) {
        msg_Err(p_this, "cannot spawn http host thread");
        net_ListenClose(p_sys->pi_listen_fd);
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_access->p_sys = p_sys;

    p_access->pf_write = Write;
    p_access->pf_seek  = Seek;
    p_access->pf_control = Control;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    net_ListenClose(p_sys->pi_listen_fd);
    net_Close(p_sys->i_socket);

    vlc_cancel(p_sys->thread);
    vlc_join(p_sys->thread, NULL);

    delete p_sys;
    msg_Dbg( p_access, "simplehttpd access output closed" );
}

static int Control( sout_access_out_t *p_access, int i_query, va_list args )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    switch( i_query )
    {
        case ACCESS_OUT_CONTROLS_PACE:
        {
            bool *pb = va_arg( args, bool * );
            *pb = true;
            break;
        }

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Write: standard write on a file descriptor.
 *****************************************************************************/
static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    size_t i_write = 0;
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    block_t *p_temp;
    while( p_buffer )
    {
        if( ( p_buffer->i_flags & BLOCK_FLAG_HEADER ) )
        {
            ssize_t writevalue = 0; //writeSegment( p_access );
            if( unlikely( writevalue < 0 ) )
            {
                block_ChainRelease ( p_buffer );
                return -1;
            }
            i_write += writevalue;
        }

        p_temp = p_buffer->p_next;
        p_buffer->p_next = NULL;
        //block_ChainAppend( &p_sys->block_buffer, p_buffer );
        p_buffer = p_temp;
    }

    msleep(100000);

    return i_write;
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    (void) i_pos;
    msg_Err( p_access, "simplehttpd sout access cannot seek" );
    return -1;
}

void* httpd_HostThread(void *p_this)
{
    sout_access_out_t     *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    /* loop between HTTP states */
    for (;;) {
        switch (p_sys->state)
        {
        case STATE_LISTEN:
            p_sys->i_socket = net_Accept(VLC_OBJECT(p_access), p_sys->pi_listen_fd);
            net_ListenClose(p_sys->pi_listen_fd);
            p_sys->pi_listen_fd = NULL;
            if (p_sys->i_socket < 0)
            {
                msg_Err(p_access, "failed to get a client socket");
                p_sys->state = STATE_DEAD;
            }
            else
            {
                p_sys->state = STATE_READ_METHOD;
            }
            break;

        case STATE_READ_METHOD:
            {
                std::stringstream line;
                for (;;) {
                    unsigned char c;
                    if (net_Read(VLC_OBJECT(p_access),p_sys->i_socket, &c, 1) != 1)
                    {
                        msg_Err(p_access, "failed to get the client request");
                        p_sys->state = STATE_DEAD;
                        break;
                    }
                    if (line.str().empty()) {
                        if (!strchr("\r\n\t ", c)) {
                            line << c; /* TODO try/catch OOM */
                        }
                    } else {
                        if (c == '\n') {
                            /* Request line is now complete */
                            std::string xtract;
                            std::getline(line, xtract, ' ');
                            if (xtract.empty()) { /* no URI: evil guy */
                                msg_Err(p_access, "unsupported empty HTTP method");
                                p_sys->state = STATE_DEAD;
                                break;
                            }
                            std::string http_method = xtract;

                            do
                                std::getline(line, xtract, ' '); /* skips extra spaces */
                            while (xtract.empty());
                            p_sys->http_url = xtract;

                            std::getline(line, xtract, ' ');

                            if (xtract.length() >= 7 && !memcmp(xtract.c_str(), "HTTP/1.", 7)) {
                                p_sys->i_version = atoi(xtract.c_str() + 7);
                            } else if (!memcmp(xtract.c_str(), "HTTP/", 5)) {
                                p_sys->status_text = "Unknown HTTP version";
                                p_sys->i_status = 505;
                                msg_Err(p_access, "unsupported HTTP version %s", xtract.c_str()+5);
                                p_sys->state = STATE_SEND_HEADER;
                            } else { /* yet another foreign protocol */
                                msg_Err(p_access, "unsupported protocol %s", xtract.c_str());
                                p_sys->state = STATE_DEAD;
                            }

                            if (http_method != "GET") {
                                msg_Err(p_access, "unsupported HTTP method %s", line.str().c_str());
                                p_sys->state = STATE_DEAD;
                            } else {
                                p_sys->state = STATE_READ_HEADERS;
                            }

                            break;
                        }
                        line << c; /* TODO try/catch OOM */
                    }
                }
            }
            break;

        case STATE_READ_HEADERS:
        {
            std::stringstream line;
            for (;;) {
                unsigned char c;
                if (net_Read(VLC_OBJECT(p_access),p_sys->i_socket, &c, 1) != 1)
                {
                    msg_Err(p_access, "failed to get the client request");
                    p_sys->state = STATE_DEAD;
                    break;
                }
                if (line.str().empty()) {
                    if (c == '\n') {
                        p_sys->state = STATE_SEND_HEADER;
                        p_sys->status_text = "OK";
                        p_sys->i_status = 200;
                        break;
                    }
                    if (!strchr("\r\t ", c)) {
                        line << c; /* TODO try/catch OOM */
                    }
                } else {
                    if (c == '\n') {
                        /* Header line is now complete */
                        std::string ll = line.str();
                        msg_Dbg(p_access, "discarded header line: %s", ll.c_str());
                        break;
                    }
                    line << c; /* TODO try/catch OOM */
                }
            }
        }
            break;

        case STATE_SEND_HEADER:
        {
            std::stringstream response;
            response << "HTTP/1.1 " << p_sys->i_status << ' ' << p_sys->status_text << '\r' << '\n';
            response << "Content-Type: " << p_sys->mime << '\r' << '\n';
            response << "Cache-Control: no-cache" << '\r' << '\n';
            response << "Connection: close" << '\r' << '\n';
            response << '\r' << '\n';

            std::string str = response.str();
            if (net_Write(VLC_OBJECT(p_access),p_sys->i_socket,str.c_str(), str.length())==str.length())
                p_sys->state = STATE_SEND_BODY;
            else
                p_sys->state = STATE_DEAD;
        }
            break;

        case STATE_SEND_BODY:
            msleep(100000); /* TODO */
            break;

        case STATE_DEAD:
            return NULL;
        }
    }
}

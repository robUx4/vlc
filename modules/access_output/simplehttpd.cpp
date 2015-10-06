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
#include <vlc_block.h>
#include <vlc_fs.h>
#include <vlc_strings.h>
#include <vlc_charset.h>
#include <vlc_mime.h>
#include <vlc_httpd.h>

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
    add_bool( SOUT_CFG_PREFIX "ratecontrol", false,
              RATECONTROL_TEXT, RATECONTROL_TEXT, true )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "mime", "ratecontrol",
    NULL
};

static ssize_t Write( sout_access_out_t *, block_t * );
static int Seek ( sout_access_out_t *, off_t  );
static int Control( sout_access_out_t *, int, va_list );

struct sout_access_out_sys_t
{
    httpd_host_t    *p_httpd_host;
    httpd_file_t    *p_httpd_file;
    char            *psz_mime;
    bool             b_ratecontrol;
};

static int FileCallback( httpd_file_sys_t *p_this, httpd_file_t *p_httpd_file, uint8_t *psz_request, uint8_t **pp_data, int *pi_data )
{
    return VLC_SUCCESS;
}

static int HandlerCallback( httpd_handler_sys_t *p_this, httpd_handler_t *p_httpd_handler, char *psz_url,
                            uint8_t *psz_request, int i_type, uint8_t *p_in, int i_in,
                            char *psz_remote_addr, char *psz_remote_host,
                            uint8_t **pp_data, int *pi_data )
{
    sout_access_out_t *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    *pi_data = asprintf((char**)pp_data, "HTTP/1.1 200 OK\r\n" \
                           "Content-Type: %s\r\n" \
                           "\r\n",
                           p_sys->psz_mime);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t     *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys;

    config_ChainParse( p_access, SOUT_CFG_PREFIX, ppsz_sout_options, p_access->p_cfg );

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
            msg_Info( p_access, "Consider passing --http-host=IP on the "
                                "command line instead." );

            char host[len + 1];
            strncpy( host, p_access->psz_path, len );
            host[len] = '\0';

            var_Create( p_access, "http-host", VLC_VAR_STRING );
            var_SetString( p_access, "http-host", host );
        }
        if( port != NULL )
        {
            /* int len = path - ++port;
            msg_Info( p_access, "Consider passing --%s-port=%.*s on the "
                                "command line instead.",
                      strcasecmp( p_access->psz_access, "https" )
                      ? "http" : "https", len, port ); */
            port++;

            int bind_port = atoi( port );
            if( bind_port > 0 )
            {
                const char *var = strcasecmp( p_access->psz_access, "simplehttpds" )
                                  ? "http-port" : "https-port";
                var_Create( p_access, var, VLC_VAR_INTEGER );
                var_SetInteger( p_access, var, bind_port );
            }
        }
    }
    if( !*path )
        path = "/";

    if( unlikely( !( p_sys = (sout_access_out_sys_t *) calloc ( 1, sizeof( *p_sys ) ) ) ) )
        return VLC_ENOMEM;

    p_sys->psz_mime = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "mime" );
    if (p_sys->psz_mime && p_sys->psz_mime[0] == '\0')
    {
        free(p_sys->psz_mime);
        p_sys->psz_mime = NULL;
    }
    if (p_sys->psz_mime == NULL)
        p_sys->psz_mime = xstrdup(vlc_mime_Ext2Mime(path));

    p_sys->b_ratecontrol = var_GetBool( p_access, SOUT_CFG_PREFIX "ratecontrol") ;

    p_sys->p_httpd_host = vlc_http_HostNew( VLC_OBJECT(p_access) );
    if ( unlikely( p_sys->p_httpd_host==NULL ) )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    p_sys->p_httpd_file =
        httpd_FileNew( p_sys->p_httpd_host, path, p_sys->psz_mime, NULL, NULL, FileCallback, (httpd_file_sys_t*) p_access );
    if ( p_sys->p_httpd_file == NULL )
    {
        msg_Err( p_access, "cannot add stream %s", p_access->psz_access );
        httpd_HostDelete( p_sys->p_httpd_host );

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
    httpd_FileDelete( p_sys->p_httpd_file );
    httpd_HostDelete( p_sys->p_httpd_host );
    free( p_sys->psz_mime );
    free( p_sys );
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
            *pb = !p_sys->b_ratecontrol;
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

/*****************************************************************************
 * chromecast.cpp: Chromecast demux filter module for vlc
 *****************************************************************************
 * Copyright © 2015 VideoLAN
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
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

#include "chromecast_common.h"

#include <vlc_demux.h>
#include <vlc_input.h>
#include <vlc_plugin.h>

#include <cassert>
#include <new>

static int DemuxOpen(vlc_object_t *);
static void DemuxClose(vlc_object_t *);

#define DEMUXFILTER_CFG_PREFIX "demux-filter-chromecast-"

static const char *const ppsz_sout_options[] = {
    "control", NULL
};

vlc_module_begin ()
    set_shortname( "cc_demux" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_( "chromecast demux wrapper" ) )
    set_capability( "demux_filter", 0 )
    add_shortcut( "cc_demux" )
    set_callbacks( DemuxOpen, DemuxClose )

    add_integer(DEMUXFILTER_CFG_PREFIX "control", NULL, "", "", false)

vlc_module_end ()

struct demux_filter_sys_t
{
    demux_filter_sys_t(demux_t * const demux, chromecast_common * const renderer)
        :p_demux(demux)
        ,p_renderer(renderer)
        ,i_length(-1)
        ,demuxReady(false)
    {
        input_item_t *p_item = input_GetItem( p_demux->p_input );
        if ( p_item )
        {
            char *psz_title = input_item_GetTitleFbName( p_item );
            free( psz_title );

            psz_title = input_item_GetArtworkURL( p_item );
            free( psz_title );
        }
    }

    ~demux_filter_sys_t()
    {
    }

    int Demux() {
        if (!demuxReady)
        {
            msg_Dbg(p_demux, "wait to demux");
            demuxReady = true;
            msg_Dbg(p_demux, "ready to demux");
        }

        return demux_Demux( p_demux );
    }

protected:
    demux_t       * const p_demux;
    chromecast_common  * const p_renderer;
    mtime_t       i_length;
    bool          demuxReady;
    bool          canSeek;
    /* seek time kept while waiting for the chromecast to "seek" */
    mtime_t       m_seektime;

private:
    int source_Control(int cmd, ...)
    {
        va_list ap;
        int ret;

        va_start(ap, cmd);
        ret = p_demux->pf_control(p_demux, cmd, ap);
        va_end(ap);
        return ret;
    }
};

static int Demux( demux_filter_t *p_demux_filter )
{
    return demux_FilterDemux( p_demux_filter );
}

static int Control( demux_filter_t *p_demux_filter, int i_query, va_list args)
{
    demux_filter_sys_t *p_sys = p_demux_filter->p_sys;

    return demux_FilterControl( p_demux_filter, i_query, args );
}

int DemuxOpen(vlc_object_t *p_this)
{
    demux_filter_t *p_demux = reinterpret_cast<demux_filter_t*>(p_this);
    if ( unlikely( p_demux->p_demux == NULL ) )
        return VLC_EBADVAR;

    chromecast_common *p_renderer = reinterpret_cast<chromecast_common *>(
                var_InheritAddress( p_demux->p_demux, CC_SHARED_VAR_NAME ) );
    if ( p_renderer == NULL )
    {
        msg_Warn( p_this, "using Chromecast demuxer with no sout" );
        return VLC_ENOOBJ;
    }

    demux_filter_sys_t *p_sys = new(std::nothrow) demux_filter_sys_t( p_demux->p_demux, p_renderer );
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    p_demux->p_sys = p_sys;
    return VLC_SUCCESS;
}

void DemuxClose(vlc_object_t *p_this)
{
    demux_filter_t *p_demux = reinterpret_cast<demux_filter_t*>(p_this);
    demux_filter_sys_t *p_sys = p_demux->p_sys;

    delete p_sys;
}

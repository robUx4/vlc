/*****************************************************************************
 * chromecast.c: Chromecast services discovery module
 *****************************************************************************
 * Copyright © 2015 VLC authors and VideoLAN
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_services_discovery.h>
#include <vlc_plugin.h>
#include <microdns/microdns.h>

static const char MDNS_CHROMECAST[] = "._googlecast._tcp.local";

static int Open ( vlc_object_t * );
static void Close( vlc_object_t * );

VLC_SD_PROBE_HELPER("chromecast", "Chromecasts", SD_CAT_RENDERER)

/*
 * Module descriptor
 */
vlc_module_begin ()
    add_submodule ()
    set_shortname (N_("Chromecasts"))
    set_description (N_("Chromecasts"))
    set_category (CAT_PLAYLIST)
    set_subcategory (SUBCAT_PLAYLIST_SD)
    set_capability ("services_discovery", 0)
    set_callbacks (Open, Close)
    add_shortcut ("chromecast")

    VLC_SD_PROBE_SUBMODULE

vlc_module_end ()

struct services_discovery_sys_t
{
    struct mdns_ctx *microdns_ctx;
};

/**
 * Probes and initializes.
 */
static int Open (vlc_object_t *obj)
{
    services_discovery_t *sd = (services_discovery_t *)obj;
    services_discovery_sys_t *p_sys = NULL;

    sd->p_sys = p_sys = calloc( 1, sizeof( services_discovery_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    if (mdns_init(&p_sys->microdns_ctx, MDNS_ADDR_IPV4, MDNS_PORT) >= 0)
    {
        goto error;
    }

    // TODO services_discovery_AddItem (sd, item, _("Local drives"));

    return VLC_SUCCESS;
error:
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: cleanup
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    mdns_cleanup(p_sys->microdns_ctx);

    free( p_sys );
}

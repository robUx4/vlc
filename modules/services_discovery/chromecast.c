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

static int Open( vlc_object_t * );
static void Close( vlc_object_t * );
static void *Run( void *p_sd );

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
    vlc_thread_t thread;
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

    int err;
    if (( err = mdns_init(&p_sys->microdns_ctx, MDNS_ADDR_IPV4, MDNS_PORT)) < 0)
    {
        msg_Err( obj, "Can't initialize microdns:%d", err );
        goto error;
    }

    if ( vlc_clone( &p_sys->thread, Run, obj, VLC_THREAD_PRIORITY_LOW) )
    {
        msg_Err(obj, "Can't run the lookup thread");
        goto error;
    }

    return VLC_SUCCESS;
error:
    mdns_cleanup( p_sys->microdns_ctx );
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

    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    mdns_cleanup( p_sys->microdns_ctx );

    free( p_sys );
}

static void new_entry_callback( void *p_this, int i_status, const struct rr_entry *p_entry )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;

    if (i_status < 0)
    {
        char err_str[128];
        if (mdns_strerror(i_status, err_str, sizeof(err_str)) == 0)
        {
            msg_Dbg(p_sd, "mDNS lookup error: %s", err_str);
        }
    }
    else if (p_entry != NULL && p_entry->next != NULL)
    {
        if (strlen(p_entry->next->name) >= strlen(MDNS_CHROMECAST))
        {
            char *psz_device_name = strndup( p_entry->next->name, strlen(p_entry->next->name) - strlen(MDNS_CHROMECAST) );
            char *psz_service_name = strdup( p_entry->next->name + strlen(psz_device_name) );
            if (!strcmp(psz_service_name, MDNS_CHROMECAST))
            {
                /* this is a Chromecast device */
                char *deviceIP = NULL;
                uint16_t devicePort = 0;
                while (p_entry != NULL)
                {
                    if (p_entry->type == RR_A)
                        deviceIP = strdup( p_entry->data.A.addr_str );
                    else if (p_entry->type == RR_AAAA)
                        deviceIP = strdup( p_entry->data.AAAA.addr_str );
                    else if (p_entry->type == RR_SRV)
                        devicePort = p_entry->data.SRV.port;
                    p_entry = p_entry->next;
                }
                msg_Dbg(p_sd, "Found Chromecast '%s' %s:%d", psz_device_name, deviceIP, devicePort);
                char deviceURI[64];
                snprintf(deviceURI, sizeof(deviceURI), "chromecast://%s:%d", deviceIP, devicePort);

                input_item_t *item = input_item_NewWithTypeExt (deviceURI, psz_device_name,
                                               0, NULL, 0, -1, ITEM_TYPE_RENDERER, true);

                services_discovery_AddItem (p_sd, item, _("Chromecast"));

                free( deviceIP );
            }
            free( psz_device_name );
            free( psz_service_name );
        }
    }
}

static bool should_stop_callback( void *p_this )
{
    VLC_UNUSED(p_this);
    vlc_testcancel();

    return false;
}

void *Run( void *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    int err;
    if (( err = mdns_listen( p_sys->microdns_ctx, MDNS_CHROMECAST+1, 20, &should_stop_callback, new_entry_callback, p_sd )) < 0)
    {
        char err_str[128];
        if (mdns_strerror(err, err_str, sizeof(err_str)) == 0)
            msg_Err( p_sd, "Failed to look for the target Name: %s", err_str );
        goto done;
    }

done:
    return NULL;
}

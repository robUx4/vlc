/*****************************************************************************
 * renderer.c: Renderers
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_renderer.h>
#include <vlc_interface.h>
#include <vlc_url.h>

struct vlc_renderer_item
{
    vlc_url_t url;
    char *psz_name;
    atomic_uint refs;
};

vlc_renderer_item *
vlc_renderer_item_new(const char *psz_uri, const char *psz_name)
{
    assert(psz_uri != NULL && psz_name != NULL);

    vlc_renderer_item *p_item = malloc(sizeof(vlc_renderer_item));
    if (p_item == NULL)
        return NULL;
    vlc_UrlParse(&p_item->url, psz_uri);

    if (p_item->url.psz_protocol == NULL || p_item->url.psz_host == NULL
     || (p_item->psz_name = strdup(psz_name)) == NULL)
    {
        vlc_UrlClean(&p_item->url);
        free(p_item);
        return NULL;
    }
    atomic_init(&p_item->refs, 1);
    return p_item;
}

const char *
vlc_renderer_item_name(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_name;
}

const char *
vlc_renderer_item_module_name(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->url.psz_protocol;
}

const char *
vlc_renderer_item_host(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->url.psz_host;
}

uint16_t
vlc_renderer_item_port(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->url.i_port;
}

vlc_renderer_item *
vlc_renderer_item_hold(vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    atomic_fetch_add(&p_item->refs, 1);
    return p_item;
}

void
vlc_renderer_item_release(vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    if (atomic_fetch_sub(&p_item->refs, 1) != 1)
        return;
    vlc_UrlClean(&p_item->url);
    free(p_item->psz_name);
    free(p_item);
}

VLC_API vlc_renderer *
vlc_renderer_start(playlist_t *p_playlist, const vlc_renderer_item *p_item)
{
    assert(p_playlist != NULL && p_item != NULL);

    char psz_port[27]; /* max unsigned + ",port=" */
    if (p_item->url.i_port > 0)
        sprintf(psz_port, ",port=%u", p_item->url.i_port);

    char *psz_intf_name;
    if (asprintf(&psz_intf_name, "%s{host=%s%s}",
                 p_item->url.psz_protocol, p_item->url.psz_host,
                 p_item->url.i_port > 0 ? psz_port : "") == -1)
        return NULL;

    intf_thread_t *p_intf = intf_New(p_playlist, psz_intf_name);
    free(psz_intf_name);

    /* XXX: for now, a renderer is a intf_thread */
    return (vlc_renderer *) p_intf;
}

VLC_API void
vlc_renderer_stop(playlist_t *p_playlist, vlc_renderer *p_renderer)
{
    assert(p_playlist != NULL && p_renderer != NULL);

    intf_Release(p_playlist, (intf_thread_t *)p_renderer);
}

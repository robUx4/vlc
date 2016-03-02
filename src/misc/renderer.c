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
#include <stdint.h>

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_renderer.h>
#include <vlc_modules.h>
#include <libvlc.h>
#include "../lib/libvlc_internal.h"

struct vlc_renderer_item
{
    char *psz_name;
    char *psz_uri;
    int i_flags;
    atomic_uint refs;
};

struct renderer_priv
{
    vlc_renderer        s;
    bool                b_has_input;
};

vlc_renderer_item *
vlc_renderer_item_new(const char *psz_name, const char *psz_uri, int i_flags)
{
    assert(psz_uri != NULL);
    vlc_renderer_item *p_item = NULL;
    vlc_url_t url;
    vlc_UrlParse(&url, psz_uri);

    if (url.psz_protocol == NULL || url.psz_host == NULL)
        goto error;

    p_item = calloc(1, sizeof(vlc_renderer_item));
    if (unlikely(p_item == NULL))
        goto error;

    if (psz_name == NULL && asprintf(&p_item->psz_name, "%s (%s)",
                                     url.psz_protocol, url.psz_host) == -1)
        goto error;

    if ((p_item->psz_uri = strdup(psz_uri)) == NULL)
    {
        free(p_item->psz_name);
        goto error;
    }

    p_item->i_flags = i_flags;
    atomic_init(&p_item->refs, 1);
    vlc_UrlClean(&url);
    return p_item;

error:
    vlc_UrlClean(&url);
    free(p_item);
    return NULL;
}

const char *
vlc_renderer_item_name(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_name;
}

const char *
vlc_renderer_item_uri(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_uri;
}

int
vlc_renderer_item_flags(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->i_flags;
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
    free(p_item->psz_name);
    free(p_item->psz_uri);
    free(p_item);
}

static inline struct renderer_priv *
renderer_priv(vlc_renderer *p_renderer)
{
    return (struct renderer_priv *)p_renderer;
}

static void
renderer_destructor(vlc_object_t *p_obj)
{
    vlc_renderer *p_renderer = (vlc_renderer *)p_obj;
    struct renderer_priv *p_priv = renderer_priv(p_renderer);

    if (p_priv->b_has_input)
        vlc_renderer_set_input(p_renderer, NULL);

    module_unneed(p_renderer, p_renderer->p_module);

    vlc_UrlClean(&p_renderer->target);
}

#undef vlc_renderer_new
vlc_renderer *
vlc_renderer_new(vlc_object_t *p_obj, const char *psz_renderer)
{
    assert(p_obj != NULL && psz_renderer != NULL);
    struct renderer_priv *p_priv =
        vlc_custom_create(p_obj, sizeof (*p_priv), "renderer");
    if (p_priv == NULL)
        return NULL;
    vlc_renderer *p_renderer = &p_priv->s;

    vlc_UrlParse(&p_renderer->target, psz_renderer);
    if (p_renderer->target.psz_protocol == NULL
     || p_renderer->target.psz_host == NULL)
    {
        vlc_object_release(p_obj);
        return NULL;
    }
    p_renderer->p_module = module_need(p_renderer, "renderer",
                                       p_renderer->target.psz_protocol, true);
    if (p_renderer->p_module == NULL)
    {
        vlc_UrlClean(&p_renderer->target);
        vlc_object_release(p_obj);
        return NULL;
    }
    assert(p_renderer->pf_set_input);
    vlc_object_set_destructor(p_renderer, renderer_destructor);

    return p_renderer;
}

int
vlc_renderer_set_input(vlc_renderer *p_renderer, input_thread_t *p_input)
{
    assert(p_renderer != NULL);
    struct renderer_priv *p_priv = renderer_priv(p_renderer);

    int i_ret = p_renderer->pf_set_input(p_renderer, p_input);
    if (i_ret == VLC_SUCCESS)
        p_priv->b_has_input = p_input != NULL;
    else
        p_priv->b_has_input = false;

    return i_ret;
}

bool
vlc_renderer_equals(vlc_renderer *p_renderer, const char *psz_renderer)
{
    assert(p_renderer->target.psz_protocol != NULL
        && p_renderer->target.psz_host != NULL);

    vlc_url_t url;
    vlc_UrlParse(&url, psz_renderer);
    if (url.psz_protocol == NULL || url.psz_host == NULL)
    {
        vlc_UrlClean(&url);
        return false;
    }
    const char *psz_option1 = p_renderer->target.psz_option != NULL
                            ? p_renderer->target.psz_option : "";
    const char *psz_option2 = url.psz_option != NULL ? url.psz_option : "";
    bool b_ret = !strcmp(p_renderer->target.psz_protocol, url.psz_protocol)
              && !strcmp(p_renderer->target.psz_host, url.psz_host)
              && !strcmp(psz_option1, psz_option2)
              && p_renderer->target.i_port == url.i_port;
    vlc_UrlClean(&url);
    return b_ret;
}

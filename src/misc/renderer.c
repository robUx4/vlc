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
    char *psz_module;
    char *psz_host;
    char *psz_name;
    char *psz_option;
    atomic_uint refs;
    vlc_renderer_flags e_flags;
    uint16_t i_port;
};

struct renderer_priv
{
    vlc_renderer        s;
    vlc_renderer_item * p_item;
    bool                b_has_input;
};

vlc_renderer_item *
vlc_renderer_item_new(const char *psz_name, const char *psz_module,
                      const char *psz_host, uint16_t i_port,
                      vlc_renderer_flags e_flags)
{
    assert(psz_module != NULL && psz_host != NULL);

    vlc_renderer_item *p_item = calloc(1, sizeof(vlc_renderer_item));
    if (unlikely(p_item == NULL))
        return NULL;

    if (psz_name == NULL)
        psz_name = "";
    if ((p_item->psz_module = strdup(psz_module)) == NULL
     || (p_item->psz_host = strdup(psz_host)) == NULL
     || (p_item->psz_name = strdup(psz_name)) == NULL
     || asprintf(&p_item->psz_option, "%s{host=%s,port=%u,name=%s,flags=%d}",
                 psz_module, psz_host, i_port, psz_name, e_flags) == -1)
    {
        free(p_item->psz_module);
        free(p_item->psz_host);
        free(p_item->psz_name);
        free(p_item);
        return NULL;
    }
    p_item->i_port = i_port;
    p_item->e_flags = e_flags;
    atomic_init(&p_item->refs, 1);
    return p_item;
}

static vlc_renderer_item *
renderer_item_new_from_option(const char *psz_renderer)
{
    config_chain_t *p_cfg = NULL;
    char *psz_module, *psz_host = NULL, *psz_name = NULL;
    vlc_renderer_flags e_flags = 0;
    uint16_t i_port = 0;
    free(config_ChainCreate(&psz_module, &p_cfg, psz_renderer));

    config_chain_t *p_read_cfg = p_cfg;
    while (p_read_cfg != NULL)
    {
        if (!strcmp(p_cfg->psz_name, "host"))
            psz_host = p_cfg->psz_value;
        else if (!strcmp(p_cfg->psz_name, "name"))
            psz_name = p_cfg->psz_value;
        else if (!strcmp(p_cfg->psz_name, "port"))
        {
            int i_val = atoi(p_cfg->psz_value);
            if (i_val >= 0 && i_val <= UINT16_MAX)
                i_port = i_val;
        }
        else if (!strcmp(p_cfg->psz_name, "flags"))
            e_flags = atoi(p_cfg->psz_value);
        p_read_cfg = p_read_cfg->p_next;
    }

    vlc_renderer_item *p_item = NULL;
    if (psz_module != NULL && psz_host != NULL)
        p_item = vlc_renderer_item_new(psz_name, psz_module, psz_host, i_port,
                                       e_flags);
    free(psz_module);
    config_ChainDestroy( p_cfg );

    return p_item;
}

const char *
vlc_renderer_item_name(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_name != NULL ? p_item->psz_name : p_item->psz_module;
}

bool
vlc_renderer_item_equals(const vlc_renderer_item *p_item,
                         const char *psz_module, const char *psz_host,
                         uint16_t i_port, vlc_renderer_flags e_flags)
{
    assert(p_item != NULL);
    return (p_item->i_port == i_port || !p_item->i_port || !i_port)
            && !strcmp(p_item->psz_host, psz_host)
            && !strcmp(p_item->psz_module, psz_module)
            && p_item->e_flags == e_flags;
}

const char *
vlc_renderer_item_host(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_host;
}

uint16_t
vlc_renderer_item_port(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->i_port;
}

vlc_renderer_flags
vlc_renderer_item_flags(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->e_flags;
}

const char *
vlc_renderer_item_option(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_option;
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
    free(p_item->psz_module);
    free(p_item->psz_host);
    free(p_item->psz_name);
    free(p_item->psz_option);
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

    vlc_renderer_item_release(p_priv->p_item);
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

    p_priv->p_item = renderer_item_new_from_option(psz_renderer);
    if (p_priv->p_item == NULL)
    {
        vlc_object_release(p_obj);
        return NULL;
    }
    p_renderer->p_item = p_priv->p_item;

    p_renderer->p_module = module_need(p_renderer, "renderer",
                                       p_priv->p_item->psz_module, true);
    if (p_renderer->p_module == NULL)
    {
        vlc_renderer_item_release(p_priv->p_item);
        vlc_object_release(p_obj);
        return NULL;
    }
    assert(p_renderer->pf_set_input);
    vlc_object_set_destructor(p_renderer, renderer_destructor);

    return p_renderer;
}

bool vlc_renderer_equals(const vlc_renderer *p_renderer,
                         const char *psz_renderer)
{
    return !strcmp(p_renderer->p_item->psz_option, psz_renderer);
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

int
vlc_renderer_volume_get(vlc_renderer *p_renderer, float *pf_volume)
{
    assert(p_renderer != NULL);

    if (!pf_volume)
        return VLC_EGENERIC;

    if (p_renderer->pf_volume_get == NULL)
        return VLC_ENOOBJ;

    return p_renderer->pf_volume_get(p_renderer, pf_volume);
}

int
vlc_renderer_volume_set(vlc_renderer *p_renderer, float f_volume)
{
    assert(p_renderer != NULL);

    if (p_renderer->pf_volume_set == NULL)
        return VLC_ENOOBJ;

    return p_renderer->pf_volume_set(p_renderer, f_volume);
}

int
vlc_renderer_mute_get(vlc_renderer *p_renderer, bool *pb_mute)
{
    assert(p_renderer != NULL && pb_mute != NULL);

    if (p_renderer->pf_mute_get == NULL)
        return VLC_ENOOBJ;

    return p_renderer->pf_mute_get(p_renderer, pb_mute);
}

int
vlc_renderer_mute_set(vlc_renderer *p_renderer, bool b_mute)
{
    assert(p_renderer != NULL);

    if (p_renderer->pf_mute_set == NULL)
        return VLC_ENOOBJ;

    return p_renderer->pf_mute_set(p_renderer, b_mute);
}

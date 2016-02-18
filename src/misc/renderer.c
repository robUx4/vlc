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
#include <vlc_modules.h>
#include <libvlc.h>
#include "../lib/libvlc_internal.h"

struct vlc_renderer_item
{
    char *psz_module;
    char *psz_host;
    uint16_t i_port;
    renderer_item_flags e_flags;
    char *psz_name;
    atomic_uint refs;
};

struct renderer_priv
{
    vlc_renderer        s;
    vlc_mutex_t         lock;
    bool                b_started;
    vlc_renderer_item * p_item;
};

vlc_renderer_item *
vlc_renderer_item_new(const char *psz_module, const char *psz_host,
                      uint16_t i_port, const char *psz_name, renderer_item_flags e_flags)
{
    assert(psz_module != NULL && psz_host != NULL);

    vlc_renderer_item *p_item = calloc(1, sizeof(vlc_renderer_item));
    if (p_item == NULL)
        return NULL;

    if (psz_name == NULL)
        psz_name = "";
    if ((p_item->psz_module = strdup(psz_module)) == NULL
            || (p_item->psz_host = strdup(psz_host)) == NULL
            || (p_item->psz_name = strdup(psz_name)) == NULL)
    {
        free(p_item->psz_module);
        free(p_item->psz_host);
        free(p_item);
        return NULL;
    }
    p_item->i_port = i_port;
    p_item->e_flags = e_flags;
    atomic_init(&p_item->refs, 1);
    return p_item;
}

const char *
vlc_renderer_item_name(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_name;
}

bool vlc_renderer_item_equals(const vlc_renderer_item *p_item1,
                              const vlc_renderer_item *p_item2)
{
    return p_item1 && p_item2
            && (p_item1->i_port == p_item2->i_port || !p_item1->i_port || !p_item2->i_port)
            && !strcmp(p_item1->psz_host, p_item2->psz_host)
            && !strcmp(p_item1->psz_module, p_item2->psz_module);
}

const char *
vlc_renderer_item_module_name(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_module;
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

renderer_item_flags
vlc_renderer_item_flags(const vlc_renderer_item *p_item)
{
    assert(p_item != NULL);

    return p_item->e_flags;
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
    free(p_item);
}

static inline struct renderer_priv *
renderer_priv(vlc_object_t *p_obj)
{
    struct renderer_priv *p_priv =
        (struct renderer_priv *)libvlc_priv(p_obj->p_libvlc)->p_renderer;
    assert(p_priv != NULL);
    return p_priv;
}

static void
renderer_unload_locked(struct renderer_priv *p_priv)
{
    vlc_renderer *p_renderer = &p_priv->s;
    if (p_renderer->p_module == NULL)
        return;

    if (p_priv->b_started)
        vlc_renderer_stop(p_renderer);

    module_unneed(p_renderer, p_renderer->p_module);
    p_renderer->p_module = NULL;

    vlc_renderer_item_release(p_priv->p_item);
    p_priv->p_item = NULL;
    p_renderer->p_item = NULL;

    p_renderer->pf_start = NULL;
    p_renderer->pf_stop = NULL;
    p_renderer->pf_volume_set = NULL;
    p_renderer->pf_volume_get = NULL;
    p_renderer->pf_mute_set = NULL;
    p_renderer->pf_mute_get = NULL;
}

#undef vlc_renderer_unload
void
vlc_renderer_unload(vlc_object_t *p_obj)
{
    assert(p_obj != NULL);
    struct renderer_priv *p_priv = renderer_priv(p_obj);

    vlc_mutex_lock(&p_priv->lock);
    renderer_unload_locked(p_priv);
    vlc_mutex_unlock(&p_priv->lock);
}

static int
renderer_load_locked(struct renderer_priv *p_priv, vlc_renderer_item *p_item)
{
    vlc_renderer *p_renderer = &p_priv->s;

    renderer_unload_locked(p_priv);

    p_priv->p_item = vlc_renderer_item_hold(p_item);
    p_renderer->p_item = p_priv->p_item;

    p_renderer->p_module = module_need(p_renderer, "renderer",
                                       p_item->psz_module, true);
    if (p_renderer->p_module == NULL)
    {
        vlc_renderer_item_release(p_priv->p_item);
        p_priv->p_item = NULL;
        p_renderer->p_item = NULL;
        return VLC_EGENERIC;
    }

    assert(p_renderer->pf_start);
    assert(p_renderer->pf_stop);

    return VLC_SUCCESS;
}

#undef vlc_renderer_load
int
vlc_renderer_load(vlc_object_t *p_obj, vlc_renderer_item *p_item)
{
    assert(p_obj != NULL && p_item != NULL);
    struct renderer_priv *p_priv = renderer_priv(p_obj);

    vlc_mutex_lock(&p_priv->lock);
    int i_ret = renderer_load_locked(p_priv, p_item);
    vlc_mutex_unlock(&p_priv->lock);

    return i_ret;
}

#undef vlc_renderer_get_item
vlc_renderer_item *
vlc_renderer_get_item(vlc_object_t *p_obj)
{
    assert(p_obj != NULL);
    struct renderer_priv *p_priv = renderer_priv(p_obj);

    vlc_mutex_lock(&p_priv->lock);
    vlc_renderer_item *p_item = p_priv->p_item ?
        vlc_renderer_item_hold(p_priv->p_item) : NULL;
    vlc_mutex_unlock(&p_priv->lock);

    return p_item;
}

#undef vlc_renderer_start
int
vlc_renderer_start(vlc_object_t *p_obj, input_thread_t *p_input)
{
    assert(p_obj != NULL && p_input != NULL);
    struct renderer_priv *p_priv = renderer_priv(p_obj);
    vlc_renderer *p_renderer = &p_priv->s;

    vlc_mutex_lock(&p_priv->lock);
    if (p_renderer->p_module == NULL || p_priv->b_started)
    {
        vlc_mutex_unlock(&p_priv->lock);
        return VLC_EGENERIC;
    }

    int i_ret = p_renderer->pf_start(p_renderer, p_input);
    if (i_ret == VLC_SUCCESS)
        p_priv->b_started = true;

    vlc_mutex_unlock(&p_priv->lock);
    return i_ret;
}

#undef vlc_renderer_stop
void
vlc_renderer_stop(vlc_object_t *p_obj)
{
    assert(p_obj != NULL);
    struct renderer_priv *p_priv = renderer_priv(p_obj);
    vlc_renderer *p_renderer = &p_priv->s;

    vlc_mutex_lock(&p_priv->lock);
    if (p_renderer->p_module == NULL || !p_priv->b_started)
    {
        vlc_mutex_unlock(&p_priv->lock);
        return;
    }

    p_renderer->pf_stop(p_renderer);
    p_priv->b_started = false;

    vlc_mutex_unlock(&p_priv->lock);
}

#undef vlc_renderer_volume_get
int
vlc_renderer_volume_get(vlc_object_t *p_obj, float *pf_volume)
{
    assert(p_obj != NULL);
    struct renderer_priv *p_priv = renderer_priv(p_obj);
    vlc_renderer *p_renderer = &p_priv->s;

    if (!pf_volume)
        return VLC_EGENERIC;

    vlc_mutex_lock(&p_priv->lock);
    if (p_renderer->pf_volume_get == NULL)
    {
        vlc_mutex_unlock(&p_priv->lock);
        return VLC_ENOOBJ;
    }

    int i_ret = p_renderer->pf_volume_get(p_renderer, pf_volume);
    vlc_mutex_unlock(&p_priv->lock);
    return i_ret;
}

#undef vlc_renderer_volume_set
int
vlc_renderer_volume_set(vlc_object_t *p_obj, float f_volume)
{
    assert(p_obj != NULL);
    struct renderer_priv *p_priv = renderer_priv(p_obj);
    vlc_renderer *p_renderer = &p_priv->s;

    vlc_mutex_lock(&p_priv->lock);
    if (p_renderer->pf_volume_set == NULL)
    {
        vlc_mutex_unlock(&p_priv->lock);
        return VLC_ENOOBJ;
    }

    int i_ret = p_renderer->pf_volume_set(p_renderer, f_volume);
    vlc_mutex_unlock(&p_priv->lock);
    return i_ret;
}

#undef vlc_renderer_mute_get
int
vlc_renderer_mute_get(vlc_object_t *p_obj, bool *pb_mute)
{
    assert(p_obj != NULL);
    struct renderer_priv *p_priv = renderer_priv(p_obj);
    vlc_renderer *p_renderer = &p_priv->s;

    if (!pb_mute)
        return VLC_EGENERIC;

    vlc_mutex_lock(&p_priv->lock);
    if (p_renderer->pf_mute_get == NULL)
    {
        vlc_mutex_unlock(&p_priv->lock);
        return VLC_ENOOBJ;
    }

    int i_ret = p_renderer->pf_mute_get(p_renderer, pb_mute);
    vlc_mutex_unlock(&p_priv->lock);
    return i_ret;
}

#undef vlc_renderer_mute_set
int
vlc_renderer_mute_set(vlc_object_t *p_obj, bool b_mute)
{
    assert(p_obj != NULL);
    struct renderer_priv *p_priv = renderer_priv(p_obj);
    vlc_renderer *p_renderer = &p_priv->s;

    vlc_mutex_lock(&p_priv->lock);
    if (p_renderer->pf_mute_set == NULL)
    {
        vlc_mutex_unlock(&p_priv->lock);
        return VLC_ENOOBJ;
    }

    int i_ret = p_renderer->pf_mute_set(p_renderer, b_mute);
    vlc_mutex_unlock(&p_priv->lock);
    return i_ret;
}

void
vlc_renderer_deinit(libvlc_int_t *p_libvlc)
{
    struct renderer_priv *p_priv =
        (struct renderer_priv *) libvlc_priv(p_libvlc)->p_renderer;
    vlc_renderer *p_renderer = &p_priv->s;

    vlc_mutex_lock(&p_priv->lock);
    renderer_unload_locked(p_priv);
    vlc_mutex_unlock(&p_priv->lock);

    vlc_mutex_destroy(&p_priv->lock);
    vlc_object_release(p_renderer);
    libvlc_priv(p_libvlc)->p_renderer = NULL;
}

int
vlc_renderer_init(libvlc_int_t *p_libvlc)
{
    struct renderer_priv *p_priv =
        vlc_custom_create(p_libvlc, sizeof (*p_priv), "renderer");
    if (p_priv == NULL)
        return VLC_EGENERIC;

    vlc_mutex_init(&p_priv->lock);

    vlc_renderer_item *p_item = NULL; /* TODO item_from_option(p_obj); */
    if (p_item != NULL)
    {
        int i_ret = renderer_load_locked(p_priv, p_item);
        vlc_renderer_item_release(p_item);
        if (i_ret != VLC_SUCCESS)
        {
            vlc_mutex_destroy(&p_priv->lock);
            vlc_object_release(&p_priv->s);
            return VLC_EGENERIC;
        }
    }
    libvlc_priv(p_libvlc)->p_renderer = (vlc_object_t *) p_priv;
    return VLC_SUCCESS;
}

int
libvlc_InternalSetRenderer( libvlc_int_t *p_libvlc, const char *name )
{
    struct renderer_priv *p_priv =
        (struct renderer_priv *) libvlc_priv(p_libvlc)->p_renderer;
    char *module;
    char *p_host = NULL;
    char *p_name = NULL;
    config_chain_t *p_cfg = NULL;

    if (name == NULL || name[0]=='\0')
    {
        vlc_mutex_lock(&p_priv->lock);
        renderer_unload_locked( p_priv );
        vlc_mutex_unlock(&p_priv->lock);
        return VLC_SUCCESS;
    }

    free( config_ChainCreate( &module, &p_cfg, name ) );

    config_chain_t *p_read_cfg = p_cfg;
    while (p_read_cfg)
    {
        if (!strcmp(p_cfg->psz_name, "host"))
            p_host = p_cfg->psz_value;
        if (!strcmp(p_cfg->psz_name, "name"))
            p_name = p_cfg->psz_value;
        p_read_cfg = p_read_cfg->p_next;
    }

    if (module == NULL || p_host == NULL)
    {
        free( module );
        config_ChainDestroy( p_cfg );
        return VLC_EGENERIC;
    }

    vlc_renderer_item *p_renderer_item = vlc_renderer_item_new(module, p_host, 0, p_name, 0);
    free( module );
    config_ChainDestroy( p_cfg );

    vlc_mutex_lock(&p_priv->lock);
    int i_ret = renderer_load_locked( p_priv, p_renderer_item );
    vlc_mutex_unlock(&p_priv->lock);
    return i_ret;
}

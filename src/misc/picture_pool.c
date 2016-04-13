/*****************************************************************************
 * picture_pool.c : picture pool functions
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * Copyright (C) 2009 Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 * Copyright (C) 2013-2015 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
# include "config.h"
#endif
#include <assert.h>
#include <limits.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_picture_pool.h>
#include <vlc_atomic.h>
#include <vlc_codec.h>
#include <vlc_vout_wrapper.h>
#include "picture.h"

static const uintptr_t pool_max = CHAR_BIT * sizeof (unsigned long long);

struct picture_pool_t {
    int       (*pic_lock)(picture_t *);
    void      (*pic_unlock)(picture_t *);
    vlc_mutex_t lock;
    vlc_cond_t  wait;

    bool               canceled;
    unsigned long long available;
    atomic_ushort      refs;
    unsigned short     picture_count;
    picture_t  *picture[];
};

static void picture_pool_Destroy(picture_pool_t *pool)
{
    if (atomic_fetch_sub(&pool->refs, 1) != 1)
        return;

    vlc_cond_destroy(&pool->wait);
    vlc_mutex_destroy(&pool->lock);
    vlc_free(pool);
}

void picture_pool_Release(picture_pool_t *pool)
{
    for (unsigned i = 0; i < pool->picture_count; i++)
        picture_Release(pool->picture[i]);
    picture_pool_Destroy(pool);
}

static void picture_pool_ReleasePicture(picture_t *clone)
{
    picture_priv_t *priv = (picture_priv_t *)clone;
    uintptr_t sys = (uintptr_t)priv->gc.opaque;
    picture_pool_t *pool = (void *)(sys & ~(pool_max - 1));
    unsigned offset = sys & (pool_max - 1);
    picture_t *picture = pool->picture[offset];

    free(clone);

    if (pool->pic_unlock != NULL)
        pool->pic_unlock(picture);
    picture_Release(picture);

    vlc_mutex_lock(&pool->lock);
    assert(!(pool->available & (1ULL << offset)));
    pool->available |= 1ULL << offset;
    vlc_cond_signal(&pool->wait);
    vlc_mutex_unlock(&pool->lock);

    picture_pool_Destroy(pool);
}

static picture_t *picture_pool_ClonePicture(picture_pool_t *pool,
                                            unsigned offset)
{
    picture_t *picture = pool->picture[offset];
    uintptr_t sys = ((uintptr_t)pool) + offset;
    picture_resource_t res = {
        .p_sys = picture->p_sys,
        .pf_destroy = picture_pool_ReleasePicture,
    };

    for (int i = 0; i < picture->i_planes; i++) {
        res.p[i].p_pixels = picture->p[i].p_pixels;
        res.p[i].i_lines = picture->p[i].i_lines;
        res.p[i].i_pitch = picture->p[i].i_pitch;
    }

    picture_t *clone = picture_NewFromResource(&picture->format, &res);
    if (likely(clone != NULL)) {
        ((picture_priv_t *)clone)->gc.opaque = (void *)sys;
        picture_Hold(picture);
    }
    return clone;
}

picture_pool_t *picture_pool_NewExtended(const picture_pool_configuration_t *cfg)
{
    if (unlikely(cfg->picture_count > pool_max))
        return NULL;

    picture_pool_t *pool = vlc_memalign(pool_max,
        sizeof (*pool) + cfg->picture_count * sizeof (picture_t *));
    if (unlikely(pool == NULL))
        return NULL;

    pool->pic_lock   = cfg->lock;
    pool->pic_unlock = cfg->unlock;
    vlc_mutex_init(&pool->lock);
    vlc_cond_init(&pool->wait);
    pool->available = (1ULL << cfg->picture_count) - 1;
    atomic_init(&pool->refs,  1);
    pool->picture_count = cfg->picture_count;
    memcpy(pool->picture, cfg->picture,
           cfg->picture_count * sizeof (picture_t *));
    pool->canceled = false;
    return pool;
}

picture_pool_t *picture_pool_New(unsigned count, picture_t *const *tab)
{
    picture_pool_configuration_t cfg = {
        .picture_count = count,
        .picture = tab,
    };

    return picture_pool_NewExtended(&cfg);
}

picture_pool_t *picture_pool_NewFromFormat(const video_format_t *fmt,
                                           unsigned count)
{
    picture_t *picture[count ? count : 1];
    unsigned i;

    for (i = 0; i < count; i++) {
        picture[i] = picture_NewFromFormat(fmt);
        if (picture[i] == NULL)
            goto error;
    }

    picture_pool_t *pool = picture_pool_New(count, picture);
    if (!pool)
        goto error;

    return pool;

error:
    while (i > 0)
        picture_Release(picture[--i]);
    return NULL;
}

picture_pool_t *picture_pool_Reserve(picture_pool_t *master, unsigned count)
{
    picture_t *picture[count ? count : 1];
    unsigned i;

    for (i = 0; i < count; i++) {
        picture[i] = picture_pool_Get(master);
        if (picture[i] == NULL)
            goto error;
    }

    picture_pool_t *pool = picture_pool_New(count, picture);
    if (!pool)
        goto error;

    return pool;

error:
    while (i > 0)
        picture_Release(picture[--i]);
    return NULL;
}

/** Find next (bit) set */
static int fnsll(unsigned long long x, unsigned i)
{
    if (i >= CHAR_BIT * sizeof (x))
        return 0;
    return ffsll(x & ~((1ULL << i) - 1));
}

picture_t *picture_pool_Get(picture_pool_t *pool)
{
    vlc_mutex_lock(&pool->lock);
    assert(pool->refs > 0);

    if (pool->canceled)
    {
        vlc_mutex_unlock(&pool->lock);
        return NULL;
    }

    for (unsigned i = ffsll(pool->available); i; i = fnsll(pool->available, i))
    {
        pool->available &= ~(1ULL << (i - 1));
        vlc_mutex_unlock(&pool->lock);

        picture_t *picture = pool->picture[i - 1];

        if (pool->pic_lock != NULL && pool->pic_lock(picture) != VLC_SUCCESS) {
            vlc_mutex_lock(&pool->lock);
            pool->available |= 1ULL << (i - 1);
            continue;
        }

        picture_t *clone = picture_pool_ClonePicture(pool, i - 1);
        if (clone != NULL) {
            assert(clone->p_next == NULL);
            atomic_fetch_add(&pool->refs, 1);
        }
        return clone;
    }

    vlc_mutex_unlock(&pool->lock);
    return NULL;
}

picture_t *picture_pool_Wait(picture_pool_t *pool)
{
    unsigned i;

    vlc_mutex_lock(&pool->lock);
    assert(pool->refs > 0);

    while (pool->available == 0)
    {
        if (pool->canceled)
        {
            vlc_mutex_unlock(&pool->lock);
            return NULL;
        }
        vlc_cond_wait(&pool->wait, &pool->lock);
    }

    i = ffsll(pool->available);
    assert(i > 0);
    pool->available &= ~(1ULL << (i - 1));
    vlc_mutex_unlock(&pool->lock);

    picture_t *picture = pool->picture[i - 1];

    if (pool->pic_lock != NULL && pool->pic_lock(picture) != VLC_SUCCESS) {
        vlc_mutex_lock(&pool->lock);
        pool->available |= 1ULL << (i - 1);
        vlc_cond_signal(&pool->wait);
        vlc_mutex_unlock(&pool->lock);
        return NULL;
    }

    picture_t *clone = picture_pool_ClonePicture(pool, i - 1);
    if (clone != NULL) {
        assert(clone->p_next == NULL);
        atomic_fetch_add(&pool->refs, 1);
    }
    return clone;
}

void picture_pool_Cancel(picture_pool_t *pool, bool canceled)
{
    vlc_mutex_lock(&pool->lock);
    assert(pool->refs > 0);

    pool->canceled = canceled;
    if (canceled)
        vlc_cond_broadcast(&pool->wait);
    vlc_mutex_unlock(&pool->lock);
}

unsigned picture_pool_Reset(picture_pool_t *pool)
{
    unsigned ret;

    vlc_mutex_lock(&pool->lock);
    assert(pool->refs > 0);
    ret = pool->picture_count - popcountll(pool->available);
    pool->available = (1ULL << pool->picture_count) - 1;
    pool->canceled = false;
    vlc_mutex_unlock(&pool->lock);

    return ret;
}

unsigned picture_pool_GetSize(const picture_pool_t *pool)
{
    return pool->picture_count;
}

void picture_pool_Enum(picture_pool_t *pool, void (*cb)(void *, picture_t *),
                       void *opaque)
{
    /* NOTE: So far, the pictures table cannot change after the pool is created
     * so there is no need to lock the pool mutex here. */
    for (unsigned i = 0; i < pool->picture_count; i++)
        cb(opaque, pool->picture[i]);
}

typedef struct vlc_picture_pool_handler
{

    decoder_t *p_dec;
    vlc_array_t pools;
    vlc_array_t factories;
    unsigned (*pf_get_dpb_size)(const decoder_t *);

} vlc_picture_pool_handler;

typedef struct
{
    video_format_t fmt;
    picture_pool_t *pool;
} pool_item;

typedef struct
{
    video_format_t fmt;
    unsigned       count;
} pool_query_item;

typedef struct
{
    vlc_fourcc_t          i_chroma;
    sub_chroma            *p_sub_chroma;
    size_t                i_sub_chroma_size;
    pool_picture_factory  *p_factory;
} pool_chroma_factory;

vlc_picture_pool_query *pool_HandlerQueryCreate()
{
    vlc_array_t *p_queries = malloc( sizeof(vlc_array_t) );
    if (p_queries)
    {
        vlc_array_init( p_queries );
    }
    return (vlc_picture_pool_query*) p_queries;
}

void pool_HandlerQueryDestroy( vlc_picture_pool_query *p_pool_handler )
{
    vlc_array_t* p_queries = (vlc_array_t*) p_pool_handler;
    for (int i = 0; i < vlc_array_count( p_queries ); ++i)
    {
        pool_query_item *p_item =  vlc_array_item_at_index( p_queries, i );
        free( p_item );
    }
    vlc_array_clear( p_queries );
}

static picture_pool_t *DefaultPoolCreate( vlc_object_t *p_obj, struct pool_picture_factory *p_pool_factory,
                                          const video_format_t *fmt, unsigned pool_size )
{
    VLC_UNUSED( p_obj );
    VLC_UNUSED( p_pool_factory );
    return picture_pool_NewFromFormat( fmt, pool_size );
}

static pool_picture_factory default_factory = {
    .p_opaque       = NULL,
    .pf_create_pool = DefaultPoolCreate,
};

pool_picture_factory *pool_HandlerGetFactory( vlc_picture_pool_handler *p_pool_handler,
                                              vlc_fourcc_t i_chroma,
                                              sub_chroma *p_sub_chroma,
                                              bool b_with_default,
                                              bool b_test_sub_chroma )
{
    for (int i = 0; i < vlc_array_count( &p_pool_handler->factories ); ++i)
    {
        pool_chroma_factory *p_item =  vlc_array_item_at_index( &p_pool_handler->factories, i );
        if ( p_item->i_chroma == i_chroma && ( !b_test_sub_chroma ||
             ( !p_item->i_sub_chroma_size ||
              !memcmp(p_item->p_sub_chroma, p_sub_chroma, p_item->i_sub_chroma_size)) ))
            return p_item->p_factory;
    }
    return b_with_default ? &default_factory : NULL;
}

int pool_HandlerAddFactory( vlc_picture_pool_handler *p_pool_handler,
                            vlc_fourcc_t i_chroma, sub_chroma *p_sub_chroma, size_t i_sub_chroma_size,
                            pool_picture_factory *factory )
{
    pool_chroma_factory *p_item = malloc( sizeof(*p_item) + i_sub_chroma_size );
    if (unlikely(p_item == NULL))
        return VLC_ENOMEM;
    p_item->i_chroma  = i_chroma;
    p_item->p_factory = factory;
    p_item->p_sub_chroma = (sub_chroma *) (p_item + 1);
    memcpy( p_item->p_sub_chroma, p_sub_chroma, i_sub_chroma_size );
    p_item->i_sub_chroma_size = i_sub_chroma_size;

    vlc_array_append( &p_pool_handler->factories, p_item );
    return VLC_SUCCESS;
}

void pool_HandlerRemoveFactory( vlc_picture_pool_handler *p_pool_handler,
                                vlc_fourcc_t i_chroma, sub_chroma *p_sub_chroma,
                                pool_picture_factory *factory )
{
    for (int i = 0; i < vlc_array_count( &p_pool_handler->factories ); ++i)
    {
        pool_chroma_factory *p_item =  vlc_array_item_at_index( &p_pool_handler->factories, i );
        if ( p_item->p_factory == factory && p_item->i_chroma == i_chroma &&
             ( !p_item->i_sub_chroma_size ||
               !memcmp( p_item->p_sub_chroma, p_sub_chroma, p_item->i_sub_chroma_size )) )
        {
            vlc_array_remove( &p_pool_handler->factories, i );
            break;
        }
    }
}

vlc_picture_pool_handler *pool_HandlerCreate(decoder_t *p_dec, unsigned (*pf_get_dpb_size)(const decoder_t *))
{
    vlc_picture_pool_handler *p_pool_handler = malloc( sizeof(*p_pool_handler) );
    if (p_pool_handler)
    {
        vlc_array_init( &p_pool_handler->pools );
        vlc_array_init( &p_pool_handler->factories );
        p_pool_handler->p_dec = p_dec;
        p_pool_handler->pf_get_dpb_size = pf_get_dpb_size;
    }
    return p_pool_handler;
}

void pool_HandlerDestroy( vlc_picture_pool_handler *p_pool_handler )
{
    for (int i = 0; i < vlc_array_count( &p_pool_handler->pools ); ++i)
    {
        pool_item *p_item =  vlc_array_item_at_index( &p_pool_handler->pools, i );
        free( p_item );
    }
    vlc_array_clear( &p_pool_handler->pools );

    for (int i = 0; i < vlc_array_count( &p_pool_handler->factories ); ++i)
    {
        pool_chroma_factory *p_item =  vlc_array_item_at_index( &p_pool_handler->factories, i );
        free( p_item );
    }
    vlc_array_clear( &p_pool_handler->factories );
}

unsigned pool_HandlerDBPSize(const vlc_picture_pool_handler *p_handled )
{
    return p_handled == NULL ? 0 : p_handled->pf_get_dpb_size( p_handled->p_dec );
}

int pool_HandlerAddQueryResult(vlc_picture_pool_query *queries, unsigned count, video_format_t *p_fmt)
{
    vlc_array_t *p_queries = (vlc_array_t *)queries;
    for (int i = 0; i < vlc_array_count( p_queries ); ++i)
    {
        pool_query_item *p_item =  vlc_array_item_at_index( p_queries, i );
        if ( video_format_IsSimilar( &p_item->fmt, p_fmt ) )
        {
            p_item->count += count;
            return VLC_SUCCESS;
        }
    }

    pool_query_item *p_item = malloc( sizeof(*p_item) );
    if (unlikely(p_item == NULL))
        return VLC_ENOMEM;
    p_item->fmt  = *p_fmt;
    p_item->count = count;

    vlc_array_append( p_queries, p_item );
    return VLC_SUCCESS;
}

int pool_HandlerQueryDecoder( vlc_picture_pool_handler *p_pool_handler,
                              vlc_picture_pool_query *p_queries, unsigned count )
{
    return pool_HandlerAddQueryResult( p_queries, count, &p_pool_handler->p_dec->fmt_out.video );
}

/**
 * @brief pool_HandlerQueryVout query to know how many picture_t the vout will need
 * @param p_pool_handler
 * @param p_queries
 * @param vd
 * @param display_pool_size
 * @return
 */
int pool_HandlerQueryVout( vlc_picture_pool_handler *p_pool_handler,
                           vlc_picture_pool_query *p_queries,
                           vout_display_t *vd, unsigned display_pool_size )
{
    picture_pool_t *display_pool = vout_display_Pool( vd, display_pool_size );
    if (display_pool == NULL)
        return VLC_ENOITEM;

#ifndef NDEBUG
    if ( picture_pool_GetSize( display_pool ) < display_pool_size )
        msg_Warn(vd, "Not enough display buffers in the pool, requested %d got %d",
                 display_pool_size, picture_pool_GetSize(display_pool));
#endif

    return pool_HandlerAddQueryResult( p_queries, display_pool_size, &vd->fmt);
}

int pool_HandlerCreatePools( vlc_object_t *p_obj, vlc_picture_pool_handler *p_pool_handler,
                             vlc_picture_pool_query *queries, vout_display_t *vd )
{
    vlc_array_t *p_queries = (vlc_array_t *)queries;
    vlc_array_t *p_factories = &p_pool_handler->factories;
    for (int i = 0; i < vlc_array_count( p_queries ); ++i)
    {
        pool_query_item *p_item = vlc_array_item_at_index( p_queries, i );
        for (int j = 0; j < vlc_array_count( p_factories ); ++j)
        {
            pool_chroma_factory *p_factory = vlc_array_item_at_index( p_factories, j );
            if ( p_factory->i_chroma == p_item->fmt.i_chroma &&
                 p_factory->i_sub_chroma_size == p_item->fmt.i_sub_chroma_size &&
                 ( !p_factory->i_sub_chroma_size ||
                   !memcmp( p_factory->p_sub_chroma, p_item->fmt.p_sub_chroma, p_factory->i_sub_chroma_size )) )
            {
                picture_pool_t *p_pool = p_factory->p_factory->pf_create_pool(p_obj, p_factory->p_factory, &p_item->fmt, p_item->count );
                if ( p_pool != NULL )
                {
                }
                break;
            }
        }
    }
    return VLC_SUCCESS;
}


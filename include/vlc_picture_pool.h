/*****************************************************************************
 * vlc_picture_pool.h: picture pool definitions
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
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

#ifndef VLC_PICTURE_POOL_H
#define VLC_PICTURE_POOL_H 1

/**
 * \file
 * This file defines picture pool structures and functions in vlc
 */

#include <vlc_picture.h>

/**
 * Picture pool handle
 */
typedef struct picture_pool_t picture_pool_t;

/**
 * Picture pool destroy callback
 */
typedef struct
{
    void *p_sys;
    void (*pf_destroy)(void *);
} picture_pool_gc_t;

/**
 * Picture pool configuration
 */
typedef struct {
    unsigned  picture_count;
    picture_t *const *picture;

    picture_pool_gc_t gc;

    int       (*lock)(picture_t *);
    void      (*unlock)(picture_t *);
} picture_pool_configuration_t;

/**
 * Creates a pool of preallocated pictures. Free pictures can be allocated from
 * the pool, and are returned to the pool when they are no longer referenced.
 *
 * This avoids allocating and deallocationg pictures repeatedly, and ensures
 * that memory consumption remains within limits.
 *
 * To obtain a picture from the pool, use picture_pool_Get(). To increase and
 * decrease the reference count, use picture_Hold() and picture_Release()
 * respectively.
 *
 * If defined, picture_pool_configuration_t::lock will be called before
 * a picture is used, and picture_pool_configuration_t::unlock will be called
 * as soon as a picture is returned to the pool.
 * Those callbacks can modify picture_t::p and access picture_t::p_sys.
 *
 * @return A pointer to the new pool on success, or NULL on error
 * (pictures are <b>not</b> released on error).
 */
VLC_API picture_pool_t * picture_pool_NewExtended( const picture_pool_configuration_t * ) VLC_USED;

/**
 * Creates a picture pool with pictures in a given array.
 * This is a convenience wrapper for picture_pool_NewExtended() without the
 * lock and unlock callbacks.
 *
 * @param count number of pictures in the array
 * @param tab array of pictures
 *
 * @return a pointer to the new pool on success, or NULL on error
 * (pictures are <b>not</b> released on error)
 */
VLC_API picture_pool_t * picture_pool_New(unsigned count,
                                          picture_t *const *tab) VLC_USED;

/**
 * Allocates pictures from the heap and creates a picture pool with them.
 * This is a convenience wrapper for picture_NewFromFormat() and
 * picture_pool_New().
 *
 * @param fmt video format of pictures to allocate from the heap
 * @param count number of pictures to allocate
 *
 * @return a pointer to the new pool on success, NULL on error
 */
VLC_API picture_pool_t * picture_pool_NewFromFormat(const video_format_t *fmt,
                                                    unsigned count) VLC_USED;

#if 0
/**
 * Allocates pictures from the heap and creates a picture pool with them.
 * This is a convenience wrapper for picture_NewFromFormat() and
 * picture_pool_New().
 *
 * @param fmt video format of pictures to allocate from the heap
 * @param count number of pictures to allocate
 *
 * @return a pointer to the new pool on success, NULL on error
 */
VLC_API picture_pool_t * picture_pool_NewFromFormatSys(const video_format_t *fmt,
                                                       unsigned count,
                                                       format_init_t *p_fmt_init) VLC_USED;
#endif

/**
 * Releases a pool created by picture_pool_NewExtended(), picture_pool_New()
 * or picture_pool_NewFromFormat().
 *
 * @note If there are no pending references to the pooled pictures, and the
 * picture_resource_t.pf_destroy callback was not NULL, it will be invoked.
 * Otherwise the default callback will be used.
 *
 * @warning If there are pending references (a.k.a. late pictures), the
 * pictures will remain valid until the all pending references are dropped by
 * picture_Release().
 */
VLC_API void picture_pool_Release( picture_pool_t * );

/**
 * Holds an extra reference to a pool.
 *
 * @warning The call must be balanced with picture_Release().
 */
VLC_API void picture_pool_Hold( picture_pool_t * );

/**
 * Obtains a picture from a pool if any is immediately available.
 *
 * The picture must be released with picture_Release().
 *
 * @return a picture, or NULL if all pictures in the pool are allocated
 *
 * @note This function is thread-safe.
 */
VLC_API picture_t * picture_pool_Get( picture_pool_t * ) VLC_USED;

/**
 * Enumerates all pictures in a pool, both free and allocated.
 *
 * @param cb callback to invoke once for each picture
 * @param data opaque data parameter for the callback (first argument)
 *
 * @note Allocated pictures may be accessed asynchronously by other threads.
 * Therefore, only read-only picture parameters can be read by the callback,
 * typically picture_t.p_sys.
 * Provided those rules are respected, the function is thread-safe.
 */
VLC_API void picture_pool_Enum( picture_pool_t *,
                                void (*cb)(void *, picture_t *), void *data );

/**
 * Forcefully return all pictures in the pool to free/unallocated state.
 *
 * @warning This can only be called when it is known that all pending
 * references to the picture pool are stale, e.g. a decoder failed to
 * release pictures properly when it terminated.
 *
 * @return the number of picture references that were freed
 */
unsigned picture_pool_Reset( picture_pool_t * );

/**
 * Forcefully marks one picture free if all pictures in the pool are allocated.
 *
 * @warning This is intrinsically race-prone. If the freed picture is still
 * used, video will be corrupt, and the process will likely crash.
 *
 * @bug Do not use this function. It will never work properly.
 * Fix the decoder bugs instead.
 */
void picture_pool_NonEmpty( picture_pool_t * );

/**
 * Reserves pictures from a pool and creates a new pool with those.
 *
 * When the new pool is released, pictures are returned to the master pool.
 * If the master pool was already released, pictures will be destroyed.
 *
 * @param count number of picture to reserve
 *
 * @return the new pool, or NULL if there were not enough pictures available
 * or on error
 *
 * @note This function is thread-safe (but it might return NULL if other
 * threads have already allocated too many pictures).
 */
VLC_API picture_pool_t * picture_pool_Reserve(picture_pool_t *, unsigned count)
VLC_USED;

/**
 * @return the total number of pictures in the given pool
 * @note This function is thread-safe.
 */
VLC_API unsigned picture_pool_GetSize(const picture_pool_t *);


#endif /* VLC_PICTURE_POOL_H */


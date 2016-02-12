/*****************************************************************************
 * vlc_renderer.h: VLC renderer
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

#ifndef VLC_RENDERER_H
#define VLC_RENDERER_H 1

/**
 * @defgroup vlc_renderer VLC renderer
 * @{
 * @file
 * This file declares VLC renderer structures and functions
 * @defgroup vlc_renderer_item VLC renderer items
 * @{
 */

typedef struct vlc_renderer_item vlc_renderer_item;

/**
 * Create a new renderer item
 *
 * @param psz_uri uri of the item, must be valid
 * @param psz_name name of the item, must be valid
 * @return a renderer item or NULL in case of error
 */
VLC_API vlc_renderer_item *
vlc_renderer_item_new(const char *psz_uri, const char *psz_name) VLC_USED;

/**
 * Hold a renderer item, i.e. creates a new reference
 */
VLC_API vlc_renderer_item *
vlc_renderer_item_hold(vlc_renderer_item *p_item);

/**
 * Releases a renderer item, i.e. decrements its reference counter
 */
VLC_API void
vlc_renderer_item_release(vlc_renderer_item *p_item);

/**
 * Get the name of a renderer item
 */
VLC_API const char *
vlc_renderer_item_name(const vlc_renderer_item *p_item);

/**
 * Get the name of the module that can handle this renderer item
 */
VLC_API const char *
vlc_renderer_item_module_name(const vlc_renderer_item *p_item);

/**
 * Get the host of a renderer item
 */
VLC_API const char *
vlc_renderer_item_host(const vlc_renderer_item *p_item);

/**
 * Get the port of a renderer item
 */
VLC_API uint16_t
vlc_renderer_item_port(const vlc_renderer_item *p_item);

/**
 * @}
 * @defgroup vlc_renderer_service VLC renderer functions
 * @{
 */

typedef struct vlc_renderer vlc_renderer;

/**
 * Start rendering
 *
 * This will create a vlc_renderer that will listen to the playlist events.
 * Medias that are being playing from the playlist will be streamed to the
 * renderer item
 *
 * @param p_playlist the playlist, must be valid
 * @param p_item the renderer item, must be valid
 * @return a renderer context, need to be stopped with vlc_renderer_stop()
 */
VLC_API vlc_renderer *
vlc_renderer_start(playlist_t *p_playlist, const vlc_renderer_item *p_item);

/**
 * Stop rendering
 */
VLC_API void
vlc_renderer_stop(playlist_t *p_playlist, vlc_renderer *p_renderer);

/** @} @} */

#endif

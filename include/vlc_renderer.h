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

#include <vlc_url.h>

/**
 * @defgroup vlc_renderer VLC renderer
 * @{
 * @file
 * This file declares VLC renderer structures and functions
 * @defgroup vlc_renderer_item VLC renderer items
 * @{
 */

typedef struct vlc_renderer_item vlc_renderer_item;

#define VLC_RENDERER_CAN_AUDIO 0x0001
#define VLC_RENDERER_CAN_VIDEO 0x0002

/**
 * Create a new renderer item
 *
 * @param psz_name name of the item
 * @param psz_uri uri of the renderer item, must contains a valid protocol and
 * a valid host
 * @param i_flags flags for the item
 * @return a renderer item or NULL in case of error
 */
VLC_API vlc_renderer_item *
vlc_renderer_item_new(const char *psz_name, const char *psz_uri,
                      int i_flags) VLC_USED;

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
 * Get the host of a renderer item
 */
VLC_API const char *
vlc_renderer_item_uri(const vlc_renderer_item *p_item);

/**
 * Get the flags of a renderer item
 */
VLC_API int
vlc_renderer_item_flags(const vlc_renderer_item *p_item);

/**
 * @}
 * @defgroup vlc_renderer_module VLC renderer module
 * @{
 */

typedef struct vlc_renderer vlc_renderer;
typedef struct vlc_renderer_sys vlc_renderer_sys;
struct vlc_renderer
{
    VLC_COMMON_MEMBERS
    /**
     */
    module_t            *p_module;
    vlc_renderer_sys    *p_sys;

    vlc_url_t           target;

    /**
     * Handle a new input thread.
     *
     * p_renderer->target is a valid url with a valid psz_protocol and valid
     * psz_host.
     *
     * @param p_input new input to handle, or NULL to stop the current one.
     */
    int     (*pf_set_input)(vlc_renderer *p_renderer, input_thread_t *p_input);
};

/** @} @} */

/* Internal API */

/* Release with vlc_object_release */
vlc_renderer *
vlc_renderer_new(vlc_object_t *p_obj, const char *psz_renderer);
#define vlc_renderer_new(a, b) vlc_renderer_new(VLC_OBJECT(a), b)

int
vlc_renderer_set_input(vlc_renderer *p_renderer, input_thread_t *p_input);

bool vlc_renderer_equals(vlc_renderer *p_renderer, const char *psz_renderer);

#endif

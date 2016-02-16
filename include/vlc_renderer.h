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

/* Called from src/libvlc.c */
vlc_renderer *
vlc_renderer_create(vlc_object_t *p_parent);

/* Called from src/libvlc.c */
void
vlc_renderer_release(vlc_renderer *);

/**
 * @defgroup vlc_renderer VLC renderer
 * @{
 * @file
 * This file declares VLC renderer structures and functions
 * @defgroup vlc_renderer_item VLC renderer items
 * @{
 */

typedef struct vlc_renderer_item vlc_renderer_item;

/** Renderer flags */
typedef enum {
    RENDERER_CAN_AUDIO         = 0x0001,  /**< Renderer can render audio */
    RENDERER_CAN_VIDEO         = 0x0002,  /**< Renderer can render video */
} renderer_item_flags;

/**
 * Create a new renderer item
 *
 * @param psz_uri uri of the item, must be valid
 * @param psz_module name of the module to use with this renderer, must be valid
 * @param psz_host IP of the item, must be valid
 * @param i_port TCP/UDP port of the item, must be valid
 * @param psz_name name of the item, must be valid
 * @param e_flags flags for the item
 * @return a renderer item or NULL in case of error
 */
VLC_API vlc_renderer_item *
vlc_renderer_item_new(const char *psz_module, const char *psz_host,
                      uint16_t i_port, const char *psz_name,
                      renderer_item_flags e_flags) VLC_USED;

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
 * Get the flags of a renderer item
 */
VLC_API renderer_item_flags
vlc_renderer_item_flags(const vlc_renderer_item *p_item);

/**
 * @}
 * @defgroup vlc_renderer_service VLC renderer functions
 * @{
 */

typedef struct vlc_renderer vlc_renderer;

/**
 * Create renderer
 *
 * TODO
 *
 * @param p_parent the parent object 
 * @param p_item the renderer item, must be valid
 * @return a renderer context, need to be stopped with vlc_renderer_stop()
 */
VLC_API vlc_renderer *
vlc_renderer_load(vlc_object_t *p_parent, vlc_renderer_item *p_item);
#define vlc_renderer_load(a) vlc_renderer_load(VLC_OBJECT(a), b)

/**
 * TODO
 */
VLC_API void
vlc_renderer_unload(vlc_renderer *p_renderer);

/**
 * TODO
 * to be released with vlc_renderer_item_release()
 */
VLC_API vlc_renderer_item *
vlc_renderer_get_item(vlc_renderer *p_renderer);

/**
 * Start rendering
 */
VLC_API int
vlc_renderer_start(vlc_renderer *p_renderer, input_thread_t *p_input);

/**
 * Stop rendering
 */
VLC_API void
vlc_renderer_stop(vlc_renderer *p_renderer);

/**
 * TODO
 */
VLC_API int
vlc_renderer_volume_change(vlc_renderer *p_renderer, int i_volume);

/**
 * TODO
 */
VLC_API int
vlc_renderer_volume_mute(vlc_renderer *p_renderer, bool b_mute);

/**
 * TODO
 */
VLC_API vlc_renderer *
vlc_renderer_get(vlc_object_t *p_obj);
#define vlc_renderer_get(a) vlc_renderer_get(VLC_OBJECT(a))

/**
 * @}
 * @defgroup vlc_renderer_module VLC renderer module
 * @{
 */

typedef struct vlc_renderer_sys vlc_renderer_sys;
struct vlc_renderer
{
    VLC_COMMON_MEMBERS
    /**
     * Open/Close callbacks:
     * int  Open(vlc_renderer *p_renderer, const vlc_renderer_item *p_item);
     * void Close(vlc_renderer *p_renderer);
     */
    module_t            *p_module;
    vlc_renderer_sys    *p_sys;

    /**
     * Called on vlc_renderer_start() or when a new input is playing
     */
    int     (*pf_start)(vlc_renderer *p_renderer, input_thread_t *p_input);
    /**
     * Called on vlc_renderer_stop() or when the input finished
     */
    void    (*pf_stop)(vlc_renderer *p_renderer);
    /**
     * Called on vlc_renderer_volume_change()
     * @param i_volume the volume in percents (0 = mute, 100 = 0dB)
     */
    int     (*pf_volume_change)(vlc_renderer *p_renderer, int i_volume);
    /**
     * Called on vlc_renderer_volume_mute()
     */
    int     (*pf_volume_mute)(vlc_renderer *p_renderer, bool b_mute);
};

/** @} @} */

#endif

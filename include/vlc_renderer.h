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

/** Renderer flags */
typedef enum {
    VLC_RENDERER_CAN_AUDIO  = 0x0001,  /**< Renderer can render audio */
    VLC_RENDERER_CAN_VIDEO  = 0x0002,  /**< Renderer can render video */
} vlc_renderer_flags;

/**
 * Create a new renderer item
 *
 * @param psz_module name of the module to use with this renderer, must be valid
 * @param psz_host Host or IP of the item, must be valid
 * @param i_port TCP/UDP port of the item, 0 for unknown port
 * @param psz_name name of the item
 * @param e_flags flags for the item
 * @return a renderer item or NULL in case of error
 */
VLC_API vlc_renderer_item *
vlc_renderer_item_new(const char *psz_name, const char *psz_module,
                      const char *psz_host, uint16_t i_port,
                      vlc_renderer_flags e_flags) VLC_USED;

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
 *  Returns true if the renderer item has the same parameters
 */
VLC_API bool
vlc_renderer_item_equals(const vlc_renderer_item *p_item,
                         const char *psz_module, const char *psz_host,
                         uint16_t i_port, vlc_renderer_flags e_flags);
/**
 * Get the name of a renderer item
 */
VLC_API const char *
vlc_renderer_item_name(const vlc_renderer_item *p_item);

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
VLC_API vlc_renderer_flags
vlc_renderer_item_flags(const vlc_renderer_item *p_item);

/**
 * Get the VLC option used to run this renderer
 */
VLC_API const char *
vlc_renderer_item_option(const vlc_renderer_item *p_item);

/**
 * @}
 * @defgroup vlc_renderer_service VLC renderer functions
 * @{
 */

/**
 * Get the volume of the loaded renderer
 *
 * @param pf_volume a pointer to the volume (0.0 to 1.0)
 * @return VLC_SUCCESS on success, VLC_ENOOBJ if no module is loaded, or an
 * other VLC error code
 */
VLC_API int
vlc_renderer_volume_get(vlc_renderer *p_renderer, float *pf_volume);

/**
 * Set the volume of the loaded renderer
 *
 * @param f_volume the volume (0.0 to 1.0)
 * @return VLC_SUCCESS on success, VLC_ENOOBJ if no module is loaded, or an
 * other VLC error code
 */
VLC_API int
vlc_renderer_volume_set(vlc_renderer *p_renderer, float f_volume);

/**
 * Get the mute state of the loaded renderer
 *
 * @return VLC_SUCCESS on success, VLC_ENOOBJ if no module is loaded, or an
 * other VLC error code
 */
VLC_API int
vlc_renderer_mute_get(vlc_renderer *p_renderer, bool *b_mute);

/**
 * Get the mute state of the loaded renderer
 *
 * @return VLC_SUCCESS on success, VLC_ENOOBJ if no module is loaded, or an
 * other VLC error code
 */
VLC_API int
vlc_renderer_mute_set(vlc_renderer *p_renderer, bool b_mute);

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

    const vlc_renderer_item *p_item;

    /**
     * Called on vlc_renderer_set_input()
     */
    int     (*pf_set_input)(vlc_renderer *p_renderer, input_thread_t *p_input);
    /**
     * Called on vlc_renderer_volume_get()
     */
    int     (*pf_volume_get)(vlc_renderer *p_renderer, float *pf_volume);
    /**
     * Called on vlc_renderer_volume_set()
     */
    int     (*pf_volume_set)(vlc_renderer *p_renderer, float f_volume);
    /**
     * Called on vlc_renderer_mute_get()
     */
    int     (*pf_mute_get)(vlc_renderer *p_renderer, bool *pb_mute);
    /**
     * Called on vlc_renderer_mute_set()
     */
    int     (*pf_mute_set)(vlc_renderer *p_renderer, bool b_mute);
};

/** @} @} */

/* Release with vlc_object_release */
vlc_renderer *
vlc_renderer_new(vlc_object_t *p_obj, const char *psz_renderer);
#define vlc_renderer_new(a, b) vlc_renderer_new(VLC_OBJECT(a), b)

/* Returns true if the renderer is created from this string option */
bool
vlc_renderer_equals(const vlc_renderer *p_renderer, const char *psz_renderer);

int
vlc_renderer_set_input(vlc_renderer *p_renderer, input_thread_t *p_input);

#endif

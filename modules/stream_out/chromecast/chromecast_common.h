/*****************************************************************************
 * chromecast.cpp: Chromecast common code between modules for vlc
 *****************************************************************************
 * Copyright © 2016 VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Steve Lhomme <robux4@videolabs.io>
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

#ifndef VLC_CHROMECAST_COMMON_H
#define VLC_CHROMECAST_COMMON_H

#include <vlc_input.h>

# ifdef __cplusplus
extern "C" {
# endif

enum connection_status
{
    CHROMECAST_DISCONNECTED,
    CHROMECAST_TLS_CONNECTED,
    CHROMECAST_AUTHENTICATED,
    CHROMECAST_APP_STARTED,
    CHROMECAST_CONNECTION_DEAD,
};

typedef struct
{
    void *p_opaque;

    void (*pf_wait_app_started)(void*);
    void (*pf_set_input_state)(void*, input_state_e state);

    void (*pf_set_length)(void*, mtime_t length);
    mtime_t (*pf_get_time)(void*);
    double (*pf_get_position)(void*);
    bool (*pf_seek_to)(void*, mtime_t i_pts);
    void (*pf_wait_seek_done)(void*);
    enum connection_status (*pf_get_connection_status)(void*);

    void (*pf_set_title)(void*, const char *psz_title);
    void (*pf_set_artwork)(void*, const char *psz_artwork);

} chromecast_common;

# ifdef __cplusplus
}
# endif

#endif // VLC_CHROMECAST_COMMON_H


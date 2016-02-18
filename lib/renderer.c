/*****************************************************************************
 * renderer.c: libvlc renderer handling functions
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include "libvlc_internal.h"
#include "../src/libvlc.h"

#include <vlc/libvlc.h>

#include <vlc_renderer.h>

#include <assert.h>

int libvlc_set_renderer( libvlc_instance_t *p_instance, const char *name )
{
    if( libvlc_InternalSetRenderer( p_instance->p_libvlc_int, name ))
    {
        if( name != NULL )
            libvlc_printerr("renderer \"%s\" initialization failed", name );
        else
            libvlc_printerr("resetting the renderer failed");
        return -1;
    }
    return 0;
}

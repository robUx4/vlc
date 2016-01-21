/*****************************************************************************
 * vlc_common.h: common definitions
 * Collection of useful common types and macros definitions
 *****************************************************************************
 * Copyright (C) 1998-2011 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Rémi Denis-Courmont
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

/**
 * \file
 * This file is a collection of common definitions and types
 */

#ifndef VLC_PRECOMPILED_HEADER_H
# define VLC_PRECOMPILED_HEADER_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "vlc_common.h"

#include "vlc_es.h"
#include "vlc_demux.h"
#include "vlc_aout.h"
#include "vlc_config.h"

#include "vlc_access.h"
#include "vlc_arrays.h"
#include "vlc_aout.h"
#include "vlc_bits.h"
#include "vlc_block.h"
#include "vlc_charset.h"
#include "vlc_codec.h"
//NO #include "vlc_filter.h"
#include "vlc_fixups.h"
#include "vlc_es_out.h"
#include "vlc_fourcc.h"
#include "vlc_input.h"
//#include "vlc_interrupt.h"
#include "vlc_meta.h"
#include "vlc_mime.h"
//#include "vlc_playlist.h"
//NO #include "vlc_plugin.h"
#include "vlc_services_discovery.h"
#include "vlc_sout.h"
#include "vlc_stream.h"
#include "vlc_url.h"
#include "vlc_vout.h"

#endif /* !VLC_PRECOMPILED_HEADER_H */

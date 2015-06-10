/*****************************************************************************
 * vlc_ws2tcpip.h
 *****************************************************************************
 * Copyright © 2014 VLC authors and VideoLAN
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
 
#ifndef VLC_WS2TCPIP_H
# define VLC_WS2TCPIP_H

#ifndef _WIN32_WINNT
# error _WIN32_WINNT should have been defined
#endif

//#pragma push_macro("_WIN32_WINNT")
//#undef _WIN32_WINNT
//#define _WIN32_WINNT 0x0502

#ifdef WINAPI_FAMILY
# pragma push_macro("WINAPI_FAMILY")
# define POP_WINAPI_FAMILY
# undef WINAPI_FAMILY
#endif

//#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP

#include <ws2tcpip.h>

#ifdef POP_WINAPI_FAMILY
# pragma pop_macro("WINAPI_FAMILY")
#else
# undef WINAPI_FAMILY
#endif

//#pragma pop_macro("_WIN32_WINNT")

#endif
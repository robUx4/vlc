/*****************************************************************************
 * alarm.c: alarm() emulation
 *****************************************************************************
 * Copyright © 2016 VLC authors and VideoLAN
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
# include <config.h>
#endif

#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
HANDLE hSIG_ALARM = INVALID_HANDLE_VALUE;

static DWORD WINAPI AlarmAfterSeconds(LPVOID lpParameter)
{
    Sleep(1000 * (intptr_t)lpParameter);
    SetEvent(hSIG_ALARM);
    exit(-1);
}
#endif

unsigned int alarm(unsigned int seconds)
{
#ifdef _WIN32
    if (hSIG_ALARM == INVALID_HANDLE_VALUE)
        hSIG_ALARM = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hSIG_ALARM == INVALID_HANDLE_VALUE)
        return 0;

    CreateThread(NULL, 0, AlarmAfterSeconds, (intptr_t)seconds, 0, NULL);
    return seconds;
#else
    return 0;
#endif
}

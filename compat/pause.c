/*****************************************************************************
 * pause.c: pause() emulation
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
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
extern HANDLE hSIG_ALARM;
#endif

int pause(void)
{
#ifdef _WIN32
    if (hSIG_ALARM == INVALID_HANDLE_VALUE)
        hSIG_ALARM = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hSIG_ALARM == INVALID_HANDLE_VALUE)
        return 0;

    DWORD ret = WaitForSingleObjectEx(hSIG_ALARM, INFINITE, TRUE);
    if (ret == WAIT_IO_COMPLETION)
        exit(0); // received the APC cancel
    if (ret == WAIT_OBJECT_0)
        printf("alarm received\n");
    errno = EINTR;
    return -1;
#endif
    return EINTR;
}

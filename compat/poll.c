/*****************************************************************************
 * poll.c: poll() emulation
 *****************************************************************************
 * Copyright © 2007-2012 Rémi Denis-Courmont
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

#ifndef _WIN32
# include <sys/time.h>
# include <sys/select.h>
# include <fcntl.h>

int (poll) (struct pollfd *fds, unsigned nfds, int timeout)
{
    fd_set rdset[1], wrset[1], exset[1];
    struct timeval tv = { 0, 0 };
    int val = -1;

    FD_ZERO (rdset);
    FD_ZERO (wrset);
    FD_ZERO (exset);
    for (unsigned i = 0; i < nfds; i++)
    {
        int fd = fds[i].fd;
        if (val < fd)
            val = fd;

        /* With POSIX, FD_SET & FD_ISSET are not defined if fd is negative or
         * bigger or equal than FD_SETSIZE. That is one of the reasons why VLC
         * uses poll() rather than select(). Most POSIX systems implement
         * fd_set has a bit field with no sanity checks. This is especially bad
         * on systems (such as BSD) that have no process open files limit by
         * default, such that it is quite feasible to get fd >= FD_SETSIZE.
         * The next instructions will result in a buffer overflow if run on
         * a POSIX system, and the later FD_ISSET would perform an undefined
         * memory read. */
        if ((unsigned)fd >= FD_SETSIZE)
        {
            errno = EINVAL;
            return -1;
        }
        if (fds[i].events & POLLRDNORM)
            FD_SET (fd, rdset);
        if (fds[i].events & POLLWRNORM)
            FD_SET (fd, wrset);
        if (fds[i].events & POLLPRI)
            FD_SET (fd, exset);
    }

    if (timeout >= 0)
    {
        div_t d = div (timeout, 1000);
        tv.tv_sec = d.quot;
        tv.tv_usec = d.rem * 1000;
    }

    val = select (val + 1, rdset, wrset, exset,
                  (timeout >= 0) ? &tv : NULL);
    if (val == -1)
    {
        if (errno != EBADF)
            return -1;

        val = 0;

        for (unsigned i = 0; i < nfds; i++)
            if (fcntl (fds[i].fd, F_GETFD) == -1)
            {
                fds[i].revents = POLLNVAL;
                val++;
            }
            else
                fds[i].revents = 0;

        return val ? val : -1;
    }

    for (unsigned i = 0; i < nfds; i++)
    {
        int fd = fds[i].fd;
        fds[i].revents = (FD_ISSET (fd, rdset) ? POLLRDNORM : 0)
                       | (FD_ISSET (fd, wrset) ? POLLWRNORM : 0)
                       | (FD_ISSET (fd, exset) ? POLLPRI : 0);
    }
    return val;
}
#else
# include <windows.h>
# include <winsock2.h>

static int wait_ms(int timeout_ms);
static long tvdiff(struct timeval newer, struct timeval older);
static struct timeval tvnow(void);

int poll(struct pollfd *fds, unsigned nfds, int timeout)
{
    struct timeval pending_tv;
    struct timeval *ptimeout;
    fd_set fds_read;
    fd_set fds_write;
    fd_set fds_err;
    SOCKET maxfd;

    struct timeval initial_tv = {0, 0};
    BOOL fds_none = TRUE;
    unsigned int i;
    int pending_ms = 0;
    int error;
    int r;

    if (fds) {
        for (i = 0; i < nfds; i++) {
            if (fds[i].fd != INVALID_SOCKET) {
                fds_none = FALSE;
                break;
            }
        }
    }
    if (fds_none) {
        return wait_ms(timeout);
    }

    if (timeout > 0) {
        pending_ms = timeout;
        initial_tv = tvnow();
    }

    FD_ZERO(&fds_read);
    FD_ZERO(&fds_write);
    FD_ZERO(&fds_err);
    maxfd = INVALID_SOCKET;

    for (i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd == INVALID_SOCKET)
            continue;
        if (fds[i].events & (POLLIN|POLLOUT|POLLPRI|POLLRDNORM|POLLWRNORM|POLLRDBAND)) {
            if (fds[i].fd > maxfd)
                maxfd = fds[i].fd;
            if (fds[i].events & (POLLRDNORM|POLLIN))
                FD_SET(fds[i].fd, &fds_read);
            if (fds[i].events & (POLLWRNORM|POLLOUT))
                FD_SET(fds[i].fd, &fds_write);
            if (fds[i].events & (POLLRDBAND|POLLPRI))
                FD_SET(fds[i].fd, &fds_err);
        }
    }

    /* WinSock select() can't handle zero events. */
    if (!fds_read.fd_count && !fds_write.fd_count && !fds_err.fd_count) {
        return wait_ms(timeout);
    }

    ptimeout = (timeout < 0) ? NULL : &pending_tv;

    do {
        if (timeout > 0) {
            pending_tv.tv_sec = pending_ms / 1000;
            pending_tv.tv_usec = (pending_ms % 1000) * 1000;
        }
        else if (!timeout) {
            pending_tv.tv_sec = 0;
            pending_tv.tv_usec = 0;
        }
        r = select(maxfd + 1,
                   /* WinSock select() can't handle fd_sets with zero bits set, so
                      don't give it such arguments.
                   */
                   fds_read.fd_count ? &fds_read : NULL,
                   fds_write.fd_count ? &fds_write : NULL,
                   fds_err.fd_count ? &fds_err : NULL,
                   ptimeout);
        if (r != -1)
            break;
        error = WSAGetLastError();
        if (error && error != EINTR)
            break;
        if (timeout > 0) {
            pending_ms = timeout - (int)tvdiff(tvnow(), initial_tv);
            if (pending_ms <= 0) {
                r = 0;  /* Simulate a "call timed out" case */
                break;
            }
        }
    } while (r == -1);

    if (r < 0)
        return -1;
    if (r == 0)
        return 0;

    r = 0;
    for (i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd == INVALID_SOCKET)
            continue;
        if (FD_ISSET(fds[i].fd, &fds_read))
            fds[i].revents |= POLLIN;
        if (FD_ISSET(fds[i].fd, &fds_write))
            fds[i].revents |= POLLOUT;
        if (FD_ISSET(fds[i].fd, &fds_err))
            fds[i].revents |= POLLPRI;
        if (fds[i].revents != 0)
            r++;
    }

    return r;
}

static int wait_ms(int timeout)
{
    int r = 0;
    if (timeout < 0)
    {
        WSASetLastError( EINVAL );
        r = -1;
    }
    else if (timeout > 0 && SleepEx( timeout, TRUE ))
    {
        WSASetLastError( EINTR );
        r = -1;
    }
    return r;
}

static long tvdiff(struct timeval newer, struct timeval older)
{
    return (newer.tv_sec-older.tv_sec)*1000 + (long)(newer.tv_usec-older.tv_usec)/1000;
}

static struct timeval tvnow(void)
{
    struct timeval now;
#if !defined(_WIN32_WINNT) || !defined(_WIN32_WINNT_VISTA) || \
    (_WIN32_WINNT < _WIN32_WINNT_VISTA)
    DWORD milliseconds = GetTickCount();
    now.tv_sec = milliseconds / 1000;
    now.tv_usec = (milliseconds % 1000) * 1000;
#else
    ULONGLONG milliseconds = GetTickCount64();
    now.tv_sec = (long) (milliseconds / 1000);
    now.tv_usec = (long) (milliseconds % 1000) * 1000;
#endif

    return now;
}
#endif

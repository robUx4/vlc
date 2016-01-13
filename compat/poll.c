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

#ifndef _WIN32
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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

#include "../include/vlc_common.h"
#include "../include/vlc_threads.h"

#undef poll

static int wait_ms(int timeout_ms);

static inline void cancel_Select(void *p_data)
{
    SOCKET *p_fd_cancel = p_data;
    if ( *p_fd_cancel != INVALID_SOCKET )
        closesocket( *p_fd_cancel );
}

static inline void cancel_Cleanup(void *p_data)
{
    SOCKET *p_fd_cancel = p_data;
    if ( *p_fd_cancel != INVALID_SOCKET )
        closesocket( *p_fd_cancel );
}

int poll(struct pollfd *fds, unsigned nfds, int timeout)
{
    struct timeval pending_tv;
    struct timeval *ptimeout;
    fd_set fds_read;
    fd_set fds_write;
    fd_set fds_err;
    SOCKET maxfd;

    mtime_t initial_time;
    int pending_ms = 0;
    int error;
    int r;

    FD_ZERO(&fds_read);
    FD_ZERO(&fds_write);
    FD_ZERO(&fds_err);
    maxfd = INVALID_SOCKET;

    for (unsigned i = 0; i < nfds; i++)
    {
        fds[i].revents = 0;
        if (fds[i].fd == INVALID_SOCKET)
            continue;
        if (fds[i].events & (POLLPRI|POLLRDNORM|POLLWRNORM)) {
            if (maxfd == INVALID_SOCKET)
                maxfd = fds[i].fd;
            if (fds[i].events & POLLRDNORM)
                FD_SET(fds[i].fd, &fds_read);
            if (fds[i].events & POLLWRNORM)
                FD_SET(fds[i].fd, &fds_write);
            if (fds[i].events & POLLPRI)
                FD_SET(fds[i].fd, &fds_err);
        }
    }

    /* WinSock select() can't handle zero events. */
    if (maxfd == INVALID_SOCKET) {
        return wait_ms(timeout);
    }

    SOCKET fd_cancel = INVALID_SOCKET;
    if (timeout != 0)
    {
        /* use a dummy UDP socket to cancel interrupt infinite select() calls */
        fd_cancel = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
        if (fd_cancel != INVALID_SOCKET)
        {
            FD_SET(fd_cancel, &fds_read);
            if (fd_cancel > maxfd)
                maxfd = fd_cancel;
        }
    }

    if (timeout < 0)
        ptimeout = NULL;
    else
    {
        pending_ms = timeout;
        initial_time = mdate();
        ptimeout = &pending_tv;
    }

    /* make the select interruptible */
    vlc_cancel_push(cancel_Select, &fd_cancel);
    do {
        if (ptimeout)
        {
            ptimeout->tv_sec = pending_ms / 1000;
            ptimeout->tv_usec = (pending_ms % 1000) * 1000;
        }
        r = select(0,
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
        if (ptimeout)
        {
            pending_ms = timeout - (int)(mdate() - initial_time) / (CLOCK_FREQ / 1000);
            if (pending_ms <= 0)
            {
                r = 0;
                break;
            }
        }
    } while (r == -1);
    vlc_cancel_pop(cancel_Cleanup, &fd_cancel);

    if (fd_cancel != INVALID_SOCKET)
    {
        if (FD_ISSET(fd_cancel, &fds_read))
            r = -1;
    }

    if (r > 0)
    {
        r = 0;
        for (unsigned i = 0; i < nfds; i++)
        {
            if (FD_ISSET(fds[i].fd, &fds_read))
                fds[i].revents |= POLLIN;
            if (FD_ISSET(fds[i].fd, &fds_write))
                fds[i].revents |= POLLOUT;
            if (FD_ISSET(fds[i].fd, &fds_err))
                fds[i].revents |= POLLPRI;
            if (fds[i].revents != 0)
                r++;
        }
    }

    return r;
}

static int wait_ms(int timeout)
{
    if (timeout < 0)
    {
        errno = EINVAL;
        return -1;
    }
    if (timeout > 0 && SleepEx( timeout, TRUE ))
    {
        errno = EINTR;
        return -1;
    }
    return 0;
}
#endif

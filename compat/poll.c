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

#include "../include/vlc_common.h"
#include "../include/vlc_threads.h"

#undef poll

static inline void cancel_Select(void *p_data)
{
    SOCKET *p_fd_cancel = p_data;
    if ( *p_fd_cancel != INVALID_SOCKET )
    {
        closesocket( *p_fd_cancel );
        *p_fd_cancel = INVALID_SOCKET;
    }
}

int poll(struct pollfd *fds, unsigned nfds, int timeout)
{
    struct timeval pending_tv;
    fd_set rdset;
    fd_set wrset;
    fd_set fds_err;
    SOCKET maxfd = INVALID_SOCKET;
    SOCKET fd_cancel = INVALID_SOCKET;
    SOCKET fd_cleanup = INVALID_SOCKET;
    int count;

    if (nfds == 0)
    {    /* WSAWaitForMultipleEvents() does not allow zero events */
        if (timeout < 0)
        {
            errno = EINVAL;
            return -1;
        }
        if (timeout > 0 && SleepEx(timeout, TRUE))
        {
            errno = EINTR;
            return -1;
        }
        return 0;
    }

    WSAEVENT *evts = malloc(nfds * sizeof (WSAEVENT));
    if (evts == NULL)
        return -1; /* ENOMEM */

    FD_ZERO(&rdset);
    FD_ZERO(&wrset);
    FD_ZERO(&fds_err);

    for (unsigned i = 0; i < nfds; i++)
    {
        SOCKET fd = fds[i].fd;
        long mask = FD_CLOSE;

        fds[i].revents = 0;
        if (maxfd == INVALID_SOCKET)
            maxfd = fd;

        if (fds[i].events & POLLRDNORM)
        {
            mask |= FD_READ | FD_ACCEPT;
            FD_SET(fd, &rdset);
        }
        if (fds[i].events & POLLWRNORM)
        {
            mask |= FD_WRITE | FD_CONNECT;
            FD_SET(fd, &wrset);
        }
        if (fds[i].events & POLLPRI)
            mask |= FD_OOB;
        FD_SET(fd, &fds_err);

        evts[i] = WSACreateEvent();
        if (evts[i] == WSA_INVALID_EVENT)
        {
            while (i > 0)
                WSACloseEvent(evts[--i]);
            free(evts);
            errno = ENOMEM;
            return -1;
        }

        if (WSAEventSelect(fds[i].fd, evts[i], mask)
         && WSAGetLastError() == WSAENOTSOCK)
            fds[i].revents |= POLLNVAL;
    }

    if (timeout != 0)
    {
        /* use a dummy UDP socket to cancel interrupt infinite select() calls */
        fd_cleanup = fd_cancel = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
        if (fd_cancel != INVALID_SOCKET)
            FD_SET(fd_cancel, &rdset);
    }

    if (timeout >= 0)
    {
        pending_tv.tv_sec = timeout / 1000;
        pending_tv.tv_usec = (timeout % 1000) * 1000;
    }

    /* make the select interruptible */
    vlc_cancel_push(cancel_Select, &fd_cancel);
    count = select(0,
               /* WinSock select() can't handle fd_sets with zero bits set, so
                  don't give it such arguments.
               */
               rdset.fd_count ? &rdset : NULL,
               wrset.fd_count ? &wrset : NULL,
               fds_err.fd_count ? &fds_err : NULL,
               timeout >= 0 ? &pending_tv : NULL);
    vlc_cancel_pop();

    if (fd_cleanup != INVALID_SOCKET)
    {
        if (fd_cancel != INVALID_SOCKET)
            closesocket( fd_cancel );
        if (FD_ISSET(fd_cleanup, &rdset))
        {
            /* the select() was canceled by an APC interrupt */
            while (nfds > 0)
                WSACloseEvent(evts[--nfds]);
            free(evts);
            errno = EINTR;
            return -1;
        }
    }

    count = 0;
    for (unsigned i = 0; i < nfds; i++)
    {
        /* also check extra events that may be available */
        WSANETWORKEVENTS ne;

        if (WSAEnumNetworkEvents(fds[i].fd, evts[i], &ne))
        {
            memset(&ne, 0, sizeof (ne));
            if (WSAGetLastError() == WSAENOTSOCK)
               fds[i].revents |= POLLNVAL;
        }
        WSAEventSelect(fds[i].fd, evts[i], 0);
        WSACloseEvent(evts[i]);

        if (ne.lNetworkEvents & FD_CLOSE)
        {
            fds[i].revents |= POLLRDNORM | POLLHUP;
            if (ne.iErrorCode[FD_CLOSE_BIT] != 0)
                fds[i].revents |= POLLERR;
        }
        if (ne.lNetworkEvents & FD_OOB)
        {
            fds[i].revents |= POLLPRI;
            if (ne.iErrorCode[FD_OOB_BIT] != 0)
                fds[i].revents |= POLLERR;
        }
        if (FD_ISSET(fds[i].fd, &rdset))
            fds[i].revents |= POLLRDNORM;
        if (FD_ISSET(fds[i].fd, &wrset))
            fds[i].revents |= POLLWRNORM;
        if (FD_ISSET(fds[i].fd, &fds_err))
            fds[i].revents |= POLLERR;

        fds[i].revents &= fds[i].events | POLLERR | POLLHUP | POLLNVAL;
        count += fds[i].revents != 0;
    }

    free(evts);

    return count;
}
#endif

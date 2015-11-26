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

int poll(struct pollfd *fds, unsigned nfds, int timeout)
{
    DWORD to = (timeout >= 0) ? (DWORD)timeout : INFINITE;

    if (nfds == 0)
    {    /* WSAWaitForMultipleEvents() does not allow zero events */
        if (SleepEx(to, TRUE))
        {
            errno = EINTR;
            return -1;
        }
        return 0;
    }

    WSAEVENT *evts = malloc(nfds * sizeof (WSAEVENT));
    if (evts == NULL)
    {
        errno = ENOMEM;
        return -1; /* ENOMEM */
    }

    DWORD ret = WSA_WAIT_FAILED;
    for (unsigned i = 0; i < nfds; i++)
    {
        long mask = FD_CLOSE;

        if (fds[i].events & (POLLRDNORM|POLLIN))
            mask |= FD_READ | FD_ACCEPT | FD_OOB;

        if (fds[i].events & (POLLWRNORM|POLLOUT))
            mask |= FD_WRITE | FD_CONNECT;

        if (fds[i].events & (POLLRDBAND|POLLPRI))
            mask |= FD_OOB;

        /* discard the events we're looking for, keep the other ones */
        fds[i].revents ^= fds[i].events;

        evts[i] = WSACreateEvent();
        if (evts[i] == WSA_INVALID_EVENT)
        {
            while (i > 0)
                WSACloseEvent(evts[--i]);
            free(evts);
            errno = ENOMEM;
            return -1;
        }

        if (!WSAEventSelect(fds[i].fd, evts[i], mask) && WSAGetLastError() == WSAENOTSOCK)
            fds[i].revents |= POLLNVAL;

        if (fds[i].revents != 0 && ret == WSA_WAIT_FAILED)
        {
            ret = WSA_WAIT_EVENT_0 + i;
        }
    }

    if (ret == WSA_WAIT_FAILED)
        ret = WSAWaitForMultipleEvents(nfds, evts, FALSE, to, TRUE);

    unsigned count = 0;
    for (unsigned i = 0; i < nfds; i++)
    {
        WSANETWORKEVENTS ne;

        if (WSAEnumNetworkEvents(fds[i].fd, evts[i], &ne))
            memset(&ne, 0, sizeof (ne));
        WSAEventSelect(fds[i].fd, evts[i], 0);
        WSACloseEvent(evts[i]);

        if (ne.lNetworkEvents & FD_CONNECT)
        {
            fds[i].revents |= POLLWRNORM;
            if (ne.iErrorCode[FD_CONNECT_BIT] != 0)
                fds[i].revents |= POLLERR;
        }
        if (ne.lNetworkEvents & FD_CLOSE)
        {
            fds[i].revents |= (fds[i].events & POLLRDNORM) | POLLHUP;
            if (ne.iErrorCode[FD_CLOSE_BIT] != 0)
                fds[i].revents |= POLLERR;
        }
        if (ne.lNetworkEvents & FD_ACCEPT)
        {
            fds[i].revents |= POLLRDNORM;
            if (ne.iErrorCode[FD_ACCEPT_BIT] != 0)
                fds[i].revents |= POLLERR;
        }
        if (ne.lNetworkEvents & FD_OOB)
        {
            fds[i].revents |= POLLPRI;
            if (ne.iErrorCode[FD_OOB_BIT] != 0)
                fds[i].revents |= POLLERR;
        }
        if (ne.lNetworkEvents & FD_READ)
        {
            fds[i].revents |= POLLRDNORM;
            if (ne.iErrorCode[FD_READ_BIT] != 0)
                fds[i].revents |= POLLERR;
        }
        if (ne.lNetworkEvents & FD_WRITE)
        {
            fds[i].revents |= POLLWRNORM;
            if (ne.iErrorCode[FD_WRITE_BIT] != 0)
                fds[i].revents |= POLLERR;
        }
        count += fds[i].revents != 0;
    }

    free(evts);

    if (count == 0 && ret == WSA_WAIT_IO_COMPLETION)
    {
        errno = EINTR;
        return -1;
    }
    return count;
}
#endif

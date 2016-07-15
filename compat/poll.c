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

extern void win32_close_socket_event(int fd);

typedef struct
{
    WSAEVENT evt;
    long     mask;
} socket_evt;

static socket_evt *opened_events = NULL;
static int i_opened_events = 0;

static socket_evt *win32_get_socket_event(int fd)
{
    if ( i_opened_events <= fd )
    {
        opened_events = realloc(opened_events, (fd+1) * sizeof(socket_evt));
        if ( opened_events == NULL )
            return NULL;
        memset(&opened_events[i_opened_events], 0, (fd+1-i_opened_events) * sizeof(socket_evt) );
        i_opened_events = fd+1;
    }
    if (opened_events[fd].evt == 0)
    {
        opened_events[fd].evt  = WSACreateEvent();
        opened_events[fd].mask = 0;
    }
    return &opened_events[fd];
}

void win32_close_socket_event(int fd)
{
    if ( i_opened_events > fd && opened_events[fd].evt != 0 )
    {
        WSACloseEvent(opened_events[fd].evt);
        opened_events[fd].evt = 0;
    }
}

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

    DWORD ret = WSA_WAIT_FAILED;
    for (unsigned i = 0; i < nfds; i++)
    {
        SOCKET fd = fds[i].fd;
        fd_set rdset, wrset, exset;

        FD_ZERO(&rdset);
        FD_ZERO(&wrset);
        FD_ZERO(&exset);
        FD_SET(fd, &exset);

        if (fds[i].events & POLLRDNORM)
            FD_SET(fd, &rdset);
        if (fds[i].events & POLLWRNORM)
            FD_SET(fd, &wrset);

        fds[i].revents = 0;

        struct timeval tv = { 0, 0 };
        /* By its horrible design, WSAEnumNetworkEvents() only enumerates
         * events that were not already signaled (i.e. it is edge-triggered).
         * WSAPoll() would be better in this respect, but worse in others.
         * So use WSAEnumNetworkEvents() after manually checking for pending
         * events. */
        if (select(0, &rdset, &wrset, &exset, &tv) > 0)
        {
            if (FD_ISSET(fd, &rdset))
                fds[i].revents |= POLLRDNORM;
            if (FD_ISSET(fd, &wrset))
                fds[i].revents |= POLLWRNORM;
            if (FD_ISSET(fd, &exset))
                /* To add pain to injury, POLLERR and POLLPRI cannot be
                 * distinguished here. */
                fds[i].revents |= POLLPRI | POLLERR;
        }

        /* only report the events requested, plus the special ones */
        if (fds[i].revents & (fds[i].events | POLLERR | POLLNVAL) && ret == WSA_WAIT_FAILED)
            ret = WSA_WAIT_EVENT_0 + i;
    }

    if (ret == WSA_WAIT_FAILED && to > 0)
    {
        WSAEVENT *evts = malloc(nfds * sizeof (WSAEVENT));
        if (evts == NULL)
            return -1; /* ENOMEM */

#ifndef NDEBUG
        //Debug( L"[%d] need to wait for %d sockets\n", GetCurrentThreadId(), nfds );
#endif

        for (unsigned i = 0; i < nfds; i++)
        {
            socket_evt *sevt = win32_get_socket_event(fds[i].fd);
            if (sevt == NULL)
            {
                free(evts);
                errno = ENOMEM;
#ifndef NDEBUG
          Debug( L"[%d] ERROR on event %d/%d\n", GetCurrentThreadId(), i, nfds );
#endif
                return -1;
            }
            evts[i] = sevt->evt;
        }

        for (unsigned i = 0; i < nfds && ret == WSA_WAIT_FAILED; i++)
        {
            SOCKET fd = fds[i].fd;
            long mask = FD_CLOSE;

            if (fds[i].events & POLLRDNORM)
            {
#ifndef NDEBUG
                Debug( L"[%d] check reading on %d (%d/%d)\n", GetCurrentThreadId(), fds[i].fd, i, nfds );
#endif
                mask |= FD_READ | FD_ACCEPT;
            }
            if (fds[i].events & POLLWRNORM)
            {
#ifndef NDEBUG
                Debug( L"[%d] check writing on %d (%d/%d)\n", GetCurrentThreadId(), fds[i].fd, i, nfds );
#endif
                mask |= FD_WRITE | FD_CONNECT;
            }
            if (fds[i].events & POLLPRI)
            {
#ifndef NDEBUG
                Debug( L"[%d] check priority on %d (%d/%d)\n", GetCurrentThreadId(), fds[i].fd, i, nfds );
#endif
                mask |= FD_OOB;
            }

            socket_evt *sevt = win32_get_socket_event(fds[i].fd);
            sevt->mask = mask;

            if (WSAEventSelect(fd, evts[i], mask) && WSAGetLastError() == WSAENOTSOCK)
            {
                fds[i].revents |= POLLNVAL;
                ret = WSA_WAIT_EVENT_0 + i;
#ifndef NDEBUG
                Debug( L"[%d] select on dead socket %d (%d/%d)\n", GetCurrentThreadId(), fds[i].fd, i, nfds );
#endif
            }
        }

        if ( ret == WSA_WAIT_FAILED )
        {
#ifndef NDEBUG
            Debug( L"[%d] waiting on %d socket(s) for 0x%x ms\n", GetCurrentThreadId(), nfds, to );
#endif
            ret = WSAWaitForMultipleEvents(nfds, evts, FALSE, to, TRUE);
#ifndef NDEBUG
            Debug( L"[%d]  done waiting ret=%x\n", GetCurrentThreadId(), nfds, ret );
#endif

            for (unsigned i = 0; i < nfds; i++)
            {
                WSANETWORKEVENTS ne;

                if (ret < WSA_WAIT_EVENT_0 + i)
                    continue; /* don't read events for sockets not found */

                if (WSAEnumNetworkEvents(fds[i].fd, evts[i], &ne))
                {
                    continue;
#ifndef NDEBUG
                    Debug( L"[%d]  %d WSAEnumNetworkEvents failed! %d/%d\n", GetCurrentThreadId(), fds[i].fd, i, nfds );
#endif
                }

#ifndef NDEBUG
                socket_evt *sevt = win32_get_socket_event(fds[i].fd);
                Debug( L"[%d]  %d events got=%x mask=%x %d/%d\n", GetCurrentThreadId(), fds[i].fd, ne.lNetworkEvents & FD_ALL_EVENTS, sevt->mask, i, nfds );
#endif
                if (ne.lNetworkEvents & FD_CONNECT)
                {
#ifndef NDEBUG
                Debug( L"[%d]  %d got CONNECT on %d/%d\n", GetCurrentThreadId(), fds[i].fd, i, nfds );
#endif
                    fds[i].revents |= POLLWRNORM;
                    if (ne.iErrorCode[FD_CONNECT_BIT] != 0)
                        fds[i].revents |= POLLERR;
                }
                if (ne.lNetworkEvents & FD_CLOSE)
                {
#ifndef NDEBUG
                Debug( L"[%d]  %d got CLOSE on %d/%d\n", GetCurrentThreadId(), fds[i].fd, i, nfds );
#endif
                    fds[i].revents |= POLLRDNORM | POLLHUP;
                    if (ne.iErrorCode[FD_CLOSE_BIT] != 0)
                        fds[i].revents |= POLLERR;
                }
                if (ne.lNetworkEvents & FD_ACCEPT)
                {
#ifndef NDEBUG
                Debug( L"[%d]  %d got ACCEPT on %d/%d\n", GetCurrentThreadId(), fds[i].fd, i, nfds );
#endif
                    fds[i].revents |= POLLRDNORM;
                    if (ne.iErrorCode[FD_ACCEPT_BIT] != 0)
                        fds[i].revents |= POLLERR;
                }
                if (ne.lNetworkEvents & FD_OOB)
                {
#ifndef NDEBUG
                Debug( L"[%d]  %d got OOB on %d/%d\n", GetCurrentThreadId(), fds[i].fd, i, nfds );
#endif
                    fds[i].revents |= POLLPRI;
                    if (ne.iErrorCode[FD_OOB_BIT] != 0)
                        fds[i].revents |= POLLERR;
                }
                if (ne.lNetworkEvents & FD_READ)
                {
#ifndef NDEBUG
                Debug( L"[%d]  %d got READ on %d/%d\n", GetCurrentThreadId(), fds[i].fd, i, nfds );
#endif
                    fds[i].revents |= POLLRDNORM;
                    if (ne.iErrorCode[FD_READ_BIT] != 0)
                        fds[i].revents |= POLLERR;
                }
                if (ne.lNetworkEvents & FD_WRITE)
                {
#ifndef NDEBUG
                Debug( L"[%d]  %d got WRITE on %d/%d\n", GetCurrentThreadId(), fds[i].fd, i, nfds );
#endif
                    fds[i].revents |= POLLWRNORM;
                    if (ne.iErrorCode[FD_WRITE_BIT] != 0)
                        fds[i].revents |= POLLERR;
                }
            }
        }

        for (unsigned i = 0; i < nfds; i++)
        {
            /* unhook the event before we close it, otherwise the socket may fail */
            WSAEventSelect(fds[i].fd, evts[i], 0);
        }
        free(evts);
    }

    unsigned count = 0;
    for (unsigned i = 0; i < nfds; i++)
    {
        /* only report the events requested, plus the special ones */
        fds[i].revents &= fds[i].events | POLLERR | POLLHUP | POLLNVAL;

        count += fds[i].revents != 0;
    }

    if (count == 0 && ret == WSA_WAIT_IO_COMPLETION)
    {
        errno = EINTR;
        return -1;
    }
    return count;
}
#endif

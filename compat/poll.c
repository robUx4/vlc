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

#define CURR_THREAD GetCurrentThreadId()

struct poll_xtra
{
    WSAEVENT evt;
    fd_set rdset, wrset, exset;
};

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
        return -1; /* ENOMEM */

    wchar_t dbg[256];
#if 0 // && !defined(NDEBUG)
    wsprintf(dbg,L"%ld poll() nfds:%u timeout:%d", CURR_THREAD, nfds, timeout);
    for (unsigned n=0;n<nfds;++n)
    {
        wsprintf(dbg + wcslen(dbg),L" (%d)[%u].events=%08x", fds[n].fd, n, fds[n].events);
    }
    wsprintf(dbg + wcslen(dbg),L"\n", nfds, timeout);
    OutputDebugString(dbg);
    //OutputDebugString(dbg);
#endif

    DWORD ret = WSA_WAIT_FAILED;
    unsigned count = 0;
    for (unsigned i = 0; i < nfds; i++)
    {
        SOCKET fd = fds[i].fd;
        long mask = FD_CLOSE/* | FD_ACCEPT */;
        fd_set rdset, wrset, exset;

        FD_ZERO(&rdset);
        FD_ZERO(&wrset);
        FD_ZERO(&exset);
        FD_SET(fd, &exset);

        if (fds[i].events & (POLLRDNORM|POLLIN))
        {
            mask |= FD_READ | FD_ACCEPT | FD_OOB;
            FD_SET(fd, &rdset);
        }
        if (fds[i].events & (POLLWRNORM|POLLOUT))
        {
            mask |= FD_WRITE | FD_CONNECT | FD_OOB;
            FD_SET(fd, &wrset);
        }
        if (fds[i].events & (POLLRDBAND|POLLPRI))
            mask |= FD_OOB;

#if 0
        /* discard the events we're looking for, keep the other ones */
        fds[i].revents ^= fds[i].events;
#else
        fds[i].revents =  0;
#endif

        evts[i] = WSACreateEvent();
        if (evts[i] == WSA_INVALID_EVENT)
        {
            while (i > 0)
                WSACloseEvent(evts[--i]);
            free(evts);
            errno = ENOMEM;
            return -1;
        }

        /* do this before the select() to make sure we don't miss a change */
        if (WSAEventSelect(fds[i].fd, evts[i], mask)
             && WSAGetLastError() == WSAENOTSOCK)
            fds[i].revents |= POLLNVAL;

#if 1
        struct timeval tv = { 0, 0 };
        /* By its horrible design, WSAEnumNetworkEvents() only enumerates
         * events that were not already signaled (i.e. it is edge-triggered).
         * WSAPoll() would be better in this respect, but worse in others.
         * So use WSAEnumNetworkEvents() after manually checking for pending
         * events. */
        if (select(0, &rdset, &wrset, &exset, &tv) > 0)
        {
            if (FD_ISSET(fd, &rdset))
            {
                fds[i].revents |= POLLRDNORM;
#if 0 // && !defined(NDEBUG)
    wsprintf(dbg,L"%ld poll() evt[%u] read avail\n", CURR_THREAD, i);
    OutputDebugString(dbg);
#endif
            }
            if (FD_ISSET(fd, &wrset))
            {
                fds[i].revents |= POLLWRNORM;
#if 0 // && !defined(NDEBUG)
                wsprintf(dbg,L"%ld poll() evt[%u] write avail\n", CURR_THREAD, i);
    OutputDebugString(dbg);
#endif
            }
            if (FD_ISSET(fd, &exset))
            {
                /* To add pain to injury, POLLERR and POLLPRI cannot be
                 * distinguished here. */
                fds[i].revents |= POLLPRI | POLLERR;
#if 0 // && !defined(NDEBUG)
                wsprintf(dbg,L"%ld poll() evt[%u] error avail\n", CURR_THREAD, i);
    OutputDebugString(dbg);
#endif
            }
        }
#endif

#if 0 // && !defined(NDEBUG)
    wsprintf(dbg,L"%ld poll() evt[%u].mask:%08x\n", CURR_THREAD, i, mask);
    OutputDebugString(dbg);
#endif

        /* only report the events requested, plus the special ones */
        fds[i].revents &= fds[i].events | POLLERR | POLLHUP | POLLNVAL;

        if (fds[i].revents != 0)
        {
            //count++;
            if (ret == WSA_WAIT_FAILED)
               ret = WSA_WAIT_EVENT_0 + i;
        }
    }

    if (ret == WSA_WAIT_FAILED)
    {
        ret = WSAWaitForMultipleEvents(nfds, evts, FALSE, to, TRUE);
    }

    //if (ret < WSA_WAIT_EVENT_0+nfds)
    //do
    {
        for (unsigned i = 0; i < nfds; i++)
        {
            WSANETWORKEVENTS ne;

            if (fds[i].revents == 0)
            {
                /* events already found with select */
#if 0 // && !defined(NDEBUG)
                wsprintf(dbg,L"%ld poll() skip reading evt[%u] %08x\n", CURR_THREAD, i, fds[i].revents);
                OutputDebugString(dbg);
#endif

                if (WSAEnumNetworkEvents(fds[i].fd, evts[i], &ne))
                    memset(&ne, 0, sizeof (ne));
                /* unhook the event before we close it, otherwise the socket may fail */
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
                    fds[i].revents |= POLLRDNORM | POLLHUP;
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
                    //char cc[1];
                    //recv(fds[i].fd, cc, 1, MSG_PEEK);
                }
                if (ne.lNetworkEvents & FD_WRITE)
                {
#if 0 // && !defined(NDEBUG)
                    wsprintf(dbg,L"%ld poll() evt[%u].found write event\n", CURR_THREAD, i);
                    OutputDebugString(dbg);
#endif
                    fds[i].revents |= POLLWRNORM;
                    if (ne.iErrorCode[FD_WRITE_BIT] != 0)
                        fds[i].revents |= POLLERR;
                    //char cc[1];
                    //send(fds[i].fd, cc, 0, 0);
                }
            }
            else
            {
                /* unhook the event before we close it, otherwise the socket may fail */
                WSAEventSelect(fds[i].fd, evts[i], 0);
                WSACloseEvent(evts[i]);
            }

            /* only report the events requested, plus the special ones */
            fds[i].revents &= fds[i].events | POLLERR | POLLHUP | POLLNVAL;

            if (fds[i].revents != 0 && ret == WSA_WAIT_FAILED)
                ret = WSA_WAIT_EVENT_0 + i;

            count += fds[i].revents != 0;
        }
    }
    //}

    free(evts);

#if 0 // && !defined(NDEBUG)
    wsprintf(dbg,L"%ld poll() done ", CURR_THREAD, nfds, timeout);
    for (unsigned n=0;n<nfds;++n)
    {
        wsprintf(dbg + wcslen(dbg),L" [%u].revents=%08x", n, fds[n].revents);
    }
    wsprintf(dbg + wcslen(dbg),L"\n", nfds, timeout);
    OutputDebugString(dbg);
    //OutputDebugString(dbg);
#endif

    if (count == 0 && ret == WSA_WAIT_IO_COMPLETION)
    {
        errno = EINTR;
        return -1;
    }
    return count;
}
#endif

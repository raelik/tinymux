/*! \file slave.cpp
 * \brief This slave does iptoname conversions.
 *
 * $Id$
 *
 * The philosophy is to keep this program as simple/small as possible.  It
 * routinely performs non-vfork forks()s, so the conventional wisdom is that
 * the smaller it is, the faster it goes.  However, with modern memory
 * management support (including copy on reference paging), size is probably
 * not the issue it once was.
 */

#include "autoconf.h"
#include "config.h"

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif // HAVE_NETDB_H

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif // HAVE_NETINET_IN_H

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif // HAVE_SYS_FILE_H

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif // HAVE_SYS_IOCTL_H

#include <signal.h>
#include "slave.h"
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif // HAVE_ARPA_INET_H

#ifdef _SGI_SOURCE
#define CAST_SIGNAL_FUNC (SIG_PF)
#else
#define CAST_SIGNAL_FUNC
#endif

pid_t parent_pid;

#define MAX_STRING 1000

#ifndef INADDR_NONE
#define INADDR_NONE ((in_addr_t)-1)
#endif

//
// copy a string, returning pointer to the null terminator of dest
//
char *stpcpy(char *dest, const char *src)
{
    while ((*dest = *src))
    {
        ++dest;
        ++src;
    }
    return (dest);
}

void child_timeout_signal(int iSig)
{
    exit(1);
}

int query(char *ip)
{
    in_addr_t addr = inet_addr(ip);
    if (addr == INADDR_NONE)
    {
        return -1;
    }
    const char *pHName = ip;

#ifdef HAVE_GETHOSTBYADDR
    struct hostent *hp = gethostbyaddr((char *) &addr, sizeof(addr), AF_INET);
    if (  NULL != hp
       && strlen(hp->h_name) < MAX_STRING)
    {
        pHName = hp->h_name;
    }
#endif // HAVE_GETHOSTBYADDR

    char buf[MAX_STRING * 2];
    char *p = stpcpy(buf, ip);
    *p++ = ' ';
    p = stpcpy(p, pHName);
    *p++ = '\n';
    *p++ = '\0';

    size_t len = strlen(buf);
    ssize_t written = write(1, buf, len);
    if (  written < 0
       || len != (size_t)written)
    {
        return (-1);
    }
    return 0;
}

void alarm_signal(int iSig)
{
    struct itimerval itime;
    struct timeval interval;

    if (getppid() != parent_pid)
    {
        exit(1);
    }

    signal(SIGALRM, CAST_SIGNAL_FUNC alarm_signal);
    interval.tv_sec = 120;  // 2 minutes.
    interval.tv_usec = 0;
    itime.it_interval = interval;
    itime.it_value = interval;
    setitimer(ITIMER_REAL, &itime, 0);
}

#define MAX_CHILDREN 20
volatile int nChildrenStarted = 0;
volatile int nChildrenEndedSIGCHLD = 0;
volatile int nChildrenEndedMain = 0;

void child_signal(int iSig)
{
    // Collect the children.
    //
    while (waitpid(0, NULL, WNOHANG) > 0)
    {
        int nChildren = nChildrenStarted - nChildrenEndedSIGCHLD
            - nChildrenEndedMain;
        if (0 < nChildren)
        {
            nChildrenEndedSIGCHLD++;
        }
    }

    signal(SIGCHLD, CAST_SIGNAL_FUNC child_signal);
}

int main(int argc, char *argv[])
{
    char arg[MAX_STRING];
    int len;
    pid_t child;

    parent_pid = getppid();
    if (parent_pid == 1)
    {
        // Our real parent process is gone, and we have been inherited by the
        // init process.
        //
        exit(1);
    }

    alarm_signal(SIGALRM);
    signal(SIGCHLD, CAST_SIGNAL_FUNC child_signal);
    signal(SIGPIPE, SIG_DFL);

    for (;;)
    {
        len = read(0, arg, MAX_STRING - 1);
        if (len == 0)
        {
            break;
        }

        if (len < 0)
        {
            if (errno == EINTR)
            {
                errno = 0;
                continue;
            }
            break;
        }
        arg[len] = '\0';

        child = fork();
        switch (child)
        {
        case -1:
            exit(1);
            break;

        case 0: // child.
            {
                // We don't want to try this for more than 5 minutes.
                //
                struct itimerval itime;
                struct timeval interval;

                interval.tv_sec = 300;  // 5 minutes.
                interval.tv_usec = 0;
                itime.it_interval = interval;
                itime.it_value = interval;
                signal(SIGALRM, CAST_SIGNAL_FUNC child_timeout_signal);
                setitimer(ITIMER_REAL, &itime, 0);
            }
            exit(query(arg) != 0);
            break;
        }

        if (child > 0)
        {
            nChildrenStarted++;
        }

        int nChildren = nChildrenStarted - nChildrenEndedSIGCHLD
            - nChildrenEndedMain;

        // Collect the children.
        //
        while (waitpid(0, NULL, (nChildren < MAX_CHILDREN) ? WNOHANG : 0) > 0)
        {
            if (0 < nChildren)
            {
                nChildrenEndedMain++;
            }
        }
    }
    exit(0);
}

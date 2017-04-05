//
// Created by szy on 17-4-1.
//
#include "epoll_lib.h"
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>


static int create_and_bind (char *port);
static int make_socket_non_blocking (int sfd);
static void* accept_new_connection(int socketfd, EpollEvent &eventBase, void *arg);
static void* handle_new_connection(int fd, EpollEvent &event, void *arg);

static int create_and_bind (char *port){
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sfd;

    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* All interfaces */

    s = getaddrinfo (NULL, port, &hints, &result);
    if (s != 0)
    {
        fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        s = bind (sfd, rp->ai_addr, rp->ai_addrlen);
        if (s == 0)
        {
            /* We managed to bind successfully! */
            break;
        }

        close (sfd);
    }

    if (rp == NULL)
    {
        fprintf (stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo (result);

    return sfd;
}

static int make_socket_non_blocking (int sfd){
    int flags, s;

    flags = fcntl (sfd, F_GETFL, 0);
    if (flags == -1)
    {
        std::cerr<<"fcntl";
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl (sfd, F_SETFL, flags);
    if (s == -1)
    {
        std::cerr<<"fcntl";
        return -1;
    }

    return 0;
}

static void* accept_new_connection(int socketfd, EpollEvent &eventBase, void *arg){

    int s;
    while (1)
    {
        struct sockaddr in_addr;
        socklen_t in_len;
        int infd;
        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

        in_len = sizeof in_addr;
        infd = accept (socketfd, &in_addr, &in_len);
        if (infd == -1)
        {
            if ((errno == EAGAIN) ||
                (errno == EWOULDBLOCK))
            {
                /* We have processed all incoming
                   connections. */
                break;
            }
            else
            {
                std::cerr<<"accept failed";
                break;
            }
        }

        s = getnameinfo (&in_addr, in_len,
                         hbuf, sizeof hbuf,
                         sbuf, sizeof sbuf,
                         NI_NUMERICHOST | NI_NUMERICSERV);
        if (s == 0)
        {
            printf("Accepted connection on descriptor %d "
                           "(host=%s, port=%s)\n", infd, hbuf, sbuf);
        }

        /* Make the incoming socket non-blocking and add it to the
           list of fds to monitor. */
        s = make_socket_non_blocking (infd);
        if (s == -1)
            abort ();

        Event accpected;
        accpected.fd = infd;
        accpected.event = EPOLLIN | EPOLLET;
        accpected.FLAG = ACCEPT;
        accpected.callback = handle_new_connection;

        eventBase.addEvent(accpected);

    }
}

static void* handle_new_connection(int fd, EpollEvent &event, void *arg){
    /* We have data on the fd waiting to be read. Read and
       display it. We must read whatever data is available
       completely, as we are running in edge-triggered mode
       and won't get a notification again for the same
       data. */
    int done = 0,s=0;

    while (1)
    {
        ssize_t count;
        char buf[512];

        count = read (fd, buf, sizeof buf);
        if (count == -1)
        {
            /* If errno == EAGAIN, that means we have read all
               data. So go back to the main loop. */
            if (errno != EAGAIN)
            {
                std::cerr<<"read";
                done = 1;
            }
            break;
        }
        else if (count == 0)
        {
            /* End of file. The remote has closed the
               connection. */
            done = 1;
            break;
        }

        /* Write the buffer to standard output */
        s = write (1, buf, count);
        if (s == -1)
        {
            std::cerr<<"write";
            abort ();
        }
    }

    if (done)
    {
        printf ("Closed connection on descriptor %d\n",
                fd);

        /* Closing the descriptor will make epoll remove it
           from the set of descriptors which are monitored. */
        close (fd);
    }
}

int main (int argc, char *argv[]){
    int sfd, s;
    EpollEvent eventBase;

    if (argc != 2)
    {
        fprintf (stderr, "Usage: %s [port]\n", argv[0]);
        exit (EXIT_FAILURE);
    }

    sfd = create_and_bind (argv[1]);
    if (sfd == -1)
        abort ();

    s = make_socket_non_blocking (sfd);
    if (s == -1)
        abort ();

    s = listen (sfd, SOMAXCONN);
    if (s == -1)
    {
        std::cerr<<"listen";
        abort ();
    }

    //对socket套接字注册epoll
    Event mainEvent;
    mainEvent.fd = sfd;
    mainEvent.event = EPOLLIN | EPOLLET;
    mainEvent.FLAG = LISTEN;
    mainEvent.callback = accept_new_connection;

    eventBase.addEvent(mainEvent);


    /* The event loop */
    while (1)
        eventBase.dispatcher();

    close (sfd);

    return EXIT_SUCCESS;
}

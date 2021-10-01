#include "event.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>

#define MAX_EPOLL_FD 1024

void event_loop_init(struct event_loop *evloop)
{
    evloop->array = calloc(MAX_EPOLL_FD, sizeof(struct fd_event));
    if (!evloop->array) {
        printf("Failed to allocate fd evloop\n");
        exit(1);
    }

    evloop->epoll_fd = epoll_create(1024);
    if (-1 == evloop->epoll_fd) {
        printf("Failed to create epoll\n");
        exit(1);
    }

    evloop->ev = malloc(MAX_EPOLL_FD * sizeof(struct epoll_event));
    if (!evloop->ev) {
        printf("Failed to allocate epoll evloop\n");
        exit(0);
    }
}

void event_cycle(struct event_loop *evloop, int ms /* in milliseconds */)
{
    int value = epoll_wait(evloop->epoll_fd, evloop->ev, evloop->max_conn, ms);
    if (value <= 0)
        return;

    for (int i = 0; i < value; i++) {
        struct epoll_event *ev = &(evloop->ev[i]);
        struct fd_event *fe = &(evloop->array[ev->data.fd]);
        fe->proc(fe->args);
    }
}

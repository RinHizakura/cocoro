#include "event.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>

/* 128 is the numbers of fd reserved for the system */
#define MAX_EPOLL_FD (1024 + 128)

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

int add_fd_event(struct event_loop *evloop,
                 int fd,
                 uint32_t what,
                 event_proc_t proc,
                 void *args)
{
    if (fd >= MAX_EPOLL_FD) {
        errno = ERANGE;
        return -1;
    }

    struct fd_event *fe = &evloop->array[fd];
    int op = (fe->event_mask == 0) ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    fe->event_mask |= what;
    struct epoll_event ev = {.events = fe->event_mask, .data.fd = fd};

    if (-1 == epoll_ctl(evloop->epoll_fd, op, fd, &ev))
        return -1;

    fe->proc = proc;
    fe->args = args;

    return 0;
}

void del_fd_event(struct event_loop *evloop, int fd, uint32_t what)
{
    if (fd >= MAX_EPOLL_FD) {
        errno = ERANGE;
        return;
    }
    struct fd_event *fe = &evloop->array[fd];
    if (fe->event_mask == 0)
        return;

    fe->event_mask &= ~(what);
    struct epoll_event ev = {.events = fe->event_mask, .data.fd = fd};

    epoll_ctl(evloop->epoll_fd,
              (fe->event_mask == 0) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD, fd, &ev);
}

void event_cycle(struct event_loop *evloop, int ms /* in milliseconds */)
{
    int value = epoll_wait(evloop->epoll_fd, evloop->ev, MAX_EPOLL_FD, ms);
    if (value <= 0)
        return;

    for (int i = 0; i < value; i++) {
        struct epoll_event *ev = &(evloop->ev[i]);
        struct fd_event *fe = &(evloop->array[ev->data.fd]);
        fe->proc(fe->args);
    }
}

void event_loop_free(struct event_loop *evloop)
{
    free(evloop->array);
    free(evloop->ev);
}

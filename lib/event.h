#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>

typedef void (*event_proc_t)(void *args);

struct fd_event {
    uint32_t event_mask;
    event_proc_t proc;
    void *args;
};

struct event_loop {
    struct fd_event *array;
    int epoll_fd;
    struct epoll_event *ev;
};

int add_fd_event(struct event_loop *evloop,
                 int fd,
                 uint32_t what,
                 event_proc_t proc,
                 void *args);
void del_fd_event(struct event_loop *evloop, int fd, uint32_t what);

void event_cycle(struct event_loop *evloop, int milliseconds);
void event_loop_init(struct event_loop *evloop);
void event_loop_free(struct event_loop *evloop);

#endif

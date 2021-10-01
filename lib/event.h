#ifndef EVENT_H
#define EVENT_H

typedef void (*event_proc_t)(void *args);

struct fd_event {
    int mask;
    event_proc_t proc;
    void *args;
};

struct event_loop {
    int max_conn; /* max fd allowed */
    struct fd_event *array;
    int epoll_fd;
    struct epoll_event *ev;
};

void event_cycle(struct event_loop *evloop, int milliseconds);
void event_loop_init(struct event_loop *evloop);

#endif

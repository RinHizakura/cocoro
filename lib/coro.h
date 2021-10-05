#ifndef COCORO_H
#define COCORO_H

#include <stdio.h>

typedef void (*coro_func)(void *args);

/* APIs about the basic environment for coroutine */
void cocoro_init();
int cocoro_add_task(coro_func func, void *args);
void cocoro_run();
int cocoro_set_nonblock(int fd);
void cocoro_exit();

/* APIs to perform coroutine operation in coroutine context */
void cocoro_yield();
#define cocoro_await(cond) \
    while (!(cond)) {      \
        cocoro_yield();    \
    }

/* APIs to provide some convenient for non-blocking system call. The input file
 * descriptor should be set by 'cocoro_set_nonblock' in advance, otherwise the
 * behavior is not guaranteed. Note that coroutines who use these APIs would
 * have higher priority to be waked up */
ssize_t cocoro_read(int fd, void *buf, size_t count);
ssize_t cocoro_write(int fd, const void *buf, size_t count);
ssize_t cocoro_recv(int fd, void *buf, size_t len, int flags);
ssize_t cocoro_send(int fd, const void *buf, size_t len, int flags);

#endif

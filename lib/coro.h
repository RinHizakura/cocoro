#ifndef COCORO_H
#define COCORO_H

#include <stdio.h>

typedef void (*coro_func)(void *args);

/* API to build basic environment for coroutine */
void cocoro_init();
int cocoro_add_task(coro_func func, void *args);
void cocoro_run();
/* API to perform coroutine operation in coroutine context */
void cocoro_yield();

#define cocoro_await(cond) \
    while (!(cond)) {      \
        cocoro_yield();    \
    }

#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "coro.h"
#include "event.h"
#include "switch.h"
#include "utils/list.h"

#define COCORO_STACK_SIZE_DEFAULT 0x1000

struct cocoro_task {
    struct list_head list; /* free_pool or active linked list */
    unsigned int coro_id;
    struct context ctx;
    struct coro_stack stack;
    coro_func func;
    void *args; /* associated with coroutine function */
};

typedef void (*sched_policy_t)(struct event_loop *evloop,
                               int timeout /* in milliseconds */);

struct cocoro_sched {
    unsigned int next_coro_id;

    struct cocoro_task main_coro;
    struct cocoro_task *current;
    struct list_head idle, active, inactive;

    struct event_loop evloop;
    sched_policy_t policy;
};

static struct cocoro_sched sched;

static inline void coroutine_init(struct cocoro_task *coro)
{
    INIT_LIST_HEAD(&coro->list);
}

static struct cocoro_task *create_coroutine()
{
    /* FIXME: The availible numbers of coroutine should be limited */

    struct cocoro_task *coro = malloc(sizeof(struct cocoro_task));

    if (!coro)
        return NULL;

    /* FIXME: The stack size of each coroutine could be personalized */
    if (coro_stack_alloc(&coro->stack, COCORO_STACK_SIZE_DEFAULT)) {
        free(coro);
        return NULL;
    }

    coro->coro_id = ++sched.next_coro_id;

    return coro;
}

static struct cocoro_task *get_coroutine()
{
    struct cocoro_task *coro;

    if (!list_empty(&sched.idle)) {
        coro = list_first_entry(&sched.idle, struct cocoro_task, list);
        list_del(&coro->list);
        coroutine_init(coro);

        return coro;
    }

    coro = create_coroutine();
    if (coro)
        coroutine_init(coro);

    return coro;
}

void cocoro_init()
{
    sched.next_coro_id = 0;
    sched.current = NULL;

    INIT_LIST_HEAD(&sched.idle);
    INIT_LIST_HEAD(&sched.active);
    INIT_LIST_HEAD(&sched.inactive);

    sched.policy = event_cycle;
    event_loop_init(&sched.evloop);
}

static void coroutine_switch(struct cocoro_task *curr, struct cocoro_task *next)
{
    sched.current = next;
    context_switch(&curr->ctx, &next->ctx);
}

static __attribute__((__regparm__(1))) void coroutine_proxy(void *args)
{
    struct cocoro_task *coro = args;

    coro->func(coro->args);
    /* If the coroutine is completed, move it to idle list */
    list_add_tail(&coro->list, &sched.idle);
    coroutine_switch(sched.current, &sched.main_coro);
}

int cocoro_add_task(coro_func func, void *args)
{
    struct cocoro_task *coro = get_coroutine();
    if (!coro)
        return -1;

    coro->func = func, coro->args = args;
    coro_stack_init(&coro->ctx, &coro->stack, coroutine_proxy, coro);
    list_add_tail(&coro->list,
                  &sched.active); /* now, add the task into the active list */
    return 0;
}


static inline struct cocoro_task *get_active_coroutine()
{
    struct cocoro_task *coro;

    if (list_empty(&sched.active))
        return NULL;

    coro = list_entry(sched.active.next, struct cocoro_task, list);
    list_del_init(&coro->list);

    return coro;
}

static inline void run_active_coroutine()
{
    struct cocoro_task *coro;

    while ((coro = get_active_coroutine()))
        coroutine_switch(&sched.main_coro, coro);
}

static inline int swap_active_inactive()
{
    /* if there's no availible task in the active list, than move all
     * tasks in the inactivte list to the active list */
    if (list_empty(&sched.active))
        list_splice_tail_init(&sched.inactive, &sched.active);

    /* if there's still no task under the active list, the infinite loop for
     * coroutine scheduler would be stopped */
    if (list_empty(&sched.active))
        return -1;

    return 0;
}

void cocoro_run()
{
    for (;;) {
        run_active_coroutine();
        /* some task in the inactive list can be waked up as soon as posible if
         * its blocking condition is event-driven */
        sched.policy(&sched.evloop, 0);
        if (swap_active_inactive())
            break;
    }
}

int cocoro_set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;

    if (flags & O_NONBLOCK)
        return 0;

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    return 0;
}

void cocoro_yield()
{
    struct cocoro_task *coro = sched.current;
    /* If the coroutine is yielded, move it to the inactive list */
    list_add_tail(&coro->list, &sched.inactive);
    coroutine_switch(sched.current, &sched.main_coro);
}

static void event_wakeup(void *args)
{
    struct cocoro_task *coro = args;
    list_del_init(&coro->list);
    list_add_tail(&coro->list, &sched.active);
}

#define __SC_DECL1(t1, a1) t1 a1
#define __SC_DECL2(t2, a2, ...) t2 a2, __SC_DECL1(__VA_ARGS__)
#define __SC_DECL3(t3, a3, ...) t3 a3, __SC_DECL2(__VA_ARGS__)

#define __SC_CAST1(t1, a1) (t1) a1
#define __SC_CAST2(t2, a2, ...) (t2) a2, __SC_CAST1(__VA_ARGS__)
#define __SC_CAST3(t3, a3, ...) (t3) a3, __SC_CAST2(__VA_ARGS__)

#define COCORO_SYSCALLx(x, name, event_flag, ...)                         \
    ssize_t cocoro_##name(int fd, __SC_DECL##x(__VA_ARGS__))              \
    {                                                                     \
        ssize_t n;                                                        \
        while ((n = name(fd, __SC_CAST##x(__VA_ARGS__))) < 0) {           \
            if (EINTR == errno)                                           \
                continue;                                                 \
            if (!((EAGAIN == errno) || (EWOULDBLOCK == errno)))           \
                return -1;                                                \
            if (add_fd_event(&sched.evloop, fd, event_flag, event_wakeup, \
                             sched.current))                              \
                return -2;                                                \
            cocoro_yield();                                               \
            del_fd_event(&sched.evloop, fd, event_flag);                  \
        }                                                                 \
        return n;                                                         \
    }

#define COCORO_SYSCALL2(name, event_flag, ...) \
    COCORO_SYSCALLx(2, name, event_flag, __VA_ARGS__)
#define COCORO_SYSCALL3(name, event_flag, ...) \
    COCORO_SYSCALLx(3, name, event_flag, __VA_ARGS__)

COCORO_SYSCALL2(read, EPOLLIN, void *, buf, size_t, count);
COCORO_SYSCALL2(write, EPOLLOUT, const void *, buf, size_t, count);
COCORO_SYSCALL3(recv, EPOLLIN, void *, buf, size_t, len, int, flags);
COCORO_SYSCALL3(send, EPOLLOUT, const void *, buf, size_t, len, int, flags);

void cocoro_exit()
{
    assert(list_empty(&sched.active));
    assert(list_empty(&sched.inactive));

    struct cocoro_task *item, *tmp;

    list_for_each_entry_safe(item, tmp, &sched.idle, list)
    {
        list_del(&item->list);
        coro_stack_free(&item->stack);
        free(item);
    }

    event_loop_free(&sched.evloop);
}

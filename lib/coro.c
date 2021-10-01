#include <stdlib.h>

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

    // sched.current = NULL;

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

static inline void swap_active_inactive()
{
    struct cocoro_task *item, *tmp;

    list_for_each_entry_safe(item, tmp, &sched.inactive, list)
    {
        list_del(&item->list);
        list_add_tail(&item->list, &sched.active);
    }
}

void cocoro_run()
{
    for (;;) {
        run_active_coroutine();
        /* if there's no availible task in the active list, than swap the
         * inactivte and active one */
        if (list_empty(&sched.active)) {
            if (list_empty(&sched.inactive))
                break;
            swap_active_inactive();
        }
    }
}

void cocoro_yield()
{
    struct cocoro_task *coro = sched.current;
    /* If the coroutine is yielded, move it to the inactive list */
    list_add_tail(&coro->list, &sched.inactive);
    coroutine_switch(sched.current, &sched.main_coro);
}

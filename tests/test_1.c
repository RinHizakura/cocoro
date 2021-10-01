#include <stdio.h>
#include "lib/coro.h"

static int flag1, flag2;

static void task1(void *args __attribute__((unused)))
{
    int i = 0;

    printf("task1 start!\n");
    for (;;) {
        cocoro_await(flag2 != 0);
        printf("task 1 is running\n");
        flag2 = 0;
        flag1 = 1;

        i++;
        if (i > 10)
            break;
    }
}

static void task2(void *args __attribute__((unused)))
{
    int i = 0;

    printf("task2 start!\n");
    for (;;) {
        flag2 = 1;
        cocoro_await(flag1 != 0);
        printf("task 2 is running\n");
        flag1 = 0;

        i++;
        if (i > 10)
            break;
    }
}

int main()
{
    cocoro_init();
    cocoro_add_task(task1, NULL);
    cocoro_add_task(task2, NULL);
    cocoro_run();
    return 0;
}

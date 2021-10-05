#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "lib/coro.h"

#define queue(T, size) \
    struct {           \
        T buf[size];   \
        size_t r, w;   \
    }
#define queue_init()   \
    {                  \
        .r = 0, .w = 0 \
    }
#define queue_len(q) (sizeof((q)->buf) / sizeof((q)->buf[0]))
#define queue_cap(q) ((q)->w - (q)->r)
#define queue_empty(q) ((q)->w == (q)->r)
#define queue_full(q) (queue_cap(q) == queue_len(q))

#define queue_push(q, el) \
    (!queue_full(q) && ((q)->buf[(q)->w++ % queue_len(q)] = (el), 1))
#define queue_pop(q) \
    (queue_empty(q) ? NULL : &(q)->buf[(q)->r++ % queue_len(q)])

typedef queue(uint8_t, 4096) byte_queue_t;

struct write_loop_args {
    int fd;
    byte_queue_t *queue;
};

static void stdin_loop(void *args)
{
    byte_queue_t *out = (byte_queue_t *) args;
    uint8_t b;
    int r;

    for (;;) {
        r = cocoro_read(STDIN_FILENO, &b, 1);
        if (r == 0) {
            cocoro_await(queue_empty(out));
            break;
        }
        cocoro_await(!queue_full(out));

        queue_push(out, b);
    }
}

static void socket_write_loop(void *args)
{
    byte_queue_t *in = ((struct write_loop_args *) args)->queue;
    int fd = ((struct write_loop_args *) args)->fd;

    uint8_t *b;

    for (;;) {
        cocoro_await(!queue_empty(in));

        b = queue_pop(in);

        cocoro_send(fd, b, 1, 0);
    }
}

static void socket_read_loop(void *args)
{
    int fd = *(int *) args;

    uint8_t b;
    int r;
    for (;;) {
        r = cocoro_recv(fd, &b, 1, 0);
        if (r == 0)
            break;
        cocoro_write(STDOUT_FILENO, &b, 1);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "USAGE: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    char *host = argv[1];
    int port = atoi(argv[2]);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("Failed to created socket\n");
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr = {.s_addr = inet_addr(host)},
        .sin_port = htons(port),
    };
    connect(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

    byte_queue_t queue = queue_init();

    cocoro_init();
    cocoro_add_task(stdin_loop, &queue);
    cocoro_add_task(socket_read_loop, &fd);
    cocoro_add_task(socket_write_loop, &(struct write_loop_args){fd, &queue});
    cocoro_set_nonblock(fd);
    cocoro_run();
    cocoro_exit();
}

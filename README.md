# cocoro

This project is mainly designed for my personal use. It aims to provide friendly interface
of coroutine functionality, which is originated from
[cserv](https://github.com/sysprog21/cserv) and work in process now.

Note: Only `x86_64` architecture is supported now.

## API

The APIs to build the basic environment for couroutine are listed in following:

Function                     | Description
-----------------------------|------------------
*cocoro_init()*              | Initialize the basic environment for cocoro
*cocoro_add_task(func, args)*| Register a function 'func' with its arguments 'args' in cocoro
*cocoro_set_nonblock(fd)*    | Set the file descriptor to nonblocking I/O mode
*cocoro_run()*               | Start cocoro to schedule and run the registered coroutines
*cocoro_exit()*              | Reclaim the memory resource of cocoro

The APIs to perform coroutine operation in coroutine context are listed in following:

Function                     | Description
-----------------------------|------------------
*cocoro_yield()*             | Yield the CPU resource to other coroutines
*cocoro_await(cond)*         | Yield the CPU resource to other coroutines until `cond == true`

We also provide `cocoro_read`, `cocoro_write`, `cocoro_recv`, and `cocoro_send` now for
non-blocking system call in the coroutines. The input file descriptor for these system
call should be set by `cocoro_set_nonblock` in advance, otherwise the behavior is not
guaranteed. Note that coroutines who use these APIs would have higher priority to be
waked up than the coroutines which use `cocoro_yield` or `cocoro_await` directly.

## TODO List

- [x] await and yield implementation
- [x] Event driven scheduler
- [ ] Nested coroutine
- [ ] ...more feature / general purpose interface

## Reference

* [cserv](https://github.com/sysprog21/cserv)
* [async.h](https://github.com/naasking/async.h)
* [concurrent-programs/tinync](https://github.com/sysprog21/concurrent-programs/tree/master/tinync)

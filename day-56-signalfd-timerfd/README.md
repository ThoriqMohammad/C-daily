# Day 56 — signalfd_timerfd

Two Linux primitives that turn **asynchronous events into file
descriptors**: `signalfd` and `timerfd`.

Both follow the same idea:

> If everything is a file descriptor, I can wait on everything
> with one mechanism.

With `epoll` (Day 51), this becomes the design philosophy of
Linux taken to its logical extreme: one loop, one blocking call,
handling sockets, pipes, timers, signals, and device files all
at once.

## The Problem Both Solve

Traditional Unix has two patterns for asynchronous events:

1. **Signals.** Delivered asynchronously, interrupting whatever
   the process is doing. You install a handler with `sigaction()`
   and the kernel calls it at an arbitrary point.
2. **Timers.** `alarm()`, `setitimer()`, or `nanosleep()` —
   each blocks or interrupts with a signal.

Both are awkward in an event loop. A signal handler can't
safely call `printf`, can't safely call `malloc`, and has to
set a flag that the main loop polls. A timer needs its own
thread or its own signal handler or a `poll()` timeout.

**signalfd** and **timerfd** make both into file descriptors.
Now `read()` is the only thing you do, and `epoll` is the only
thing you wait on. No handlers, no flags, no threads.

## signalfd

A `signalfd` is a file descriptor that becomes readable when
a signal is pending:

```c
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGINT);
sigaddset(&mask, SIGUSR1);
sigprocmask(SIG_BLOCK, &mask, NULL);      /* block them */

int sfd = signalfd(-1, &mask, SFD_CLOEXEC);

struct signalfd_siginfo info;
read(sfd, &info, sizeof(info));           /* when ready */
printf("got signal %u from pid %u\n",
       info.ssi_signo, info.ssi_pid);

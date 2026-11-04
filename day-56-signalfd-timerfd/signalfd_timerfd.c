/* signalfd_timerfd.c
 *
 * Two Linux primitives that turn asynchronous events into file
 * descriptors: signalfd and timerfd.
 *
 * Both follow the same idea:
 *
 *   "If everything is a file descriptor, then I can wait on
 *    everything with one mechanism."
 *
 * With epoll (Day 51), you can wait on sockets, pipes, timers,
 * signals, and device files all at once. That's the design
 * philosophy of Linux — everything is a file — taken to its
 * logical extreme.
 *
 * signalfd:
 *   Instead of installing a signal handler with sigaction(),
 *   you create a file descriptor that becomes readable when
 *   the signal arrives. read() returns a struct signalfd_siginfo
 *   describing the signal.
 *
 * timerfd:
 *   Instead of using alarm(), setitimer(), or nanosleep(), you
 *   create a file descriptor that becomes readable when a timer
 *   expires. read() returns the number of expirations since the
 *   last read.
 *
 * Both are used together in real event loops. In this demo,
 * we wait on both through epoll and show how they arrive.
 *
 * Usage:
 *   ./signalfd_timerfd
 *
 * Skills practiced:
 *   - signalfd() with sigprocmask to block signals
 *   - struct signalfd_siginfo
 *   - timerfd_create() and timerfd_settime()
 *   - struct itimerspec for timer configuration
 *   - epoll integration of both
 *   - The "everything is a file descriptor" philosophy
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <time.h>
#include <stdint.h>

/* ---------- helpers ---------- */

static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/* ---------- Demo 1: signalfd ---------- */

static void demo_signalfd(void)
{
    printf("--- 1. signalfd: signals as a file descriptor ---\n");

    /* Step 1: block the signals we want to receive via signalfd.
     * If we don't block them, the default action fires instead
     * of them becoming "pending" for signalfd. */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
        die("sigprocmask");

    /* Step 2: create the signalfd. -1 means "create a new one".
     * SFD_CLOEXEC makes it close-on-exec, which is best practice. */
    int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (sfd == -1)
        die("signalfd");

    printf("Created signalfd (fd = %d). Sending SIGUSR1 to self...\n", sfd);

    /* Step 3: raise SIGUSR1. It's blocked, so instead of the
     * default action (terminate), it becomes pending and the
     * signalfd becomes readable. */
    if (raise(SIGUSR1) != 0)
        die("raise");

    /* Step 4: read the signal info from the signalfd. */
    struct signalfd_siginfo info;
    ssize_t n = read(sfd, &info, sizeof(info));
    if (n != sizeof(info))
        die("read signalfd");

    printf("Received signal %u (%s)\n",
           info.ssi_signo, strsignal(info.ssi_signo));
    printf("  sender pid:  %u\n", info.ssi_pid);
    printf("  sender uid:  %u\n", info.ssi_uid);

    close(sfd);
    printf("\n");
}

/* ---------- Demo 2: timerfd ---------- */

static void demo_timerfd(void)
{
    printf("--- 2. timerfd: timers as a file descriptor ---\n");

    /* Step 1: create the timerfd. CLOCK_MONOTONIC is immune to
     * wall-clock changes. TFD_CLOEXEC is best practice. */
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (tfd == -1)
        die("timerfd_create");

    /* Step 2: arm it. it_value = first expiry (relative to now
     * because TFD_TIMER_ABSTIME is not set), it_interval = repeat. */
    struct itimerspec ts;
    memset(&ts, 0, sizeof(ts));
    ts.it_value.tv_sec  = 0;
    ts.it_value.tv_nsec = 50 * 1000 * 1000;   /* 50 ms */
    ts.it_interval.tv_sec  = 0;
    ts.it_interval.tv_nsec = 50 * 1000 * 1000; /* every 50 ms */

    if (timerfd_settime(tfd, 0, &ts, NULL) == -1)
        die("timerfd_settime");

    printf("Timer armed for 50 ms, repeating. Reading 5 expirations...\n");

    /* Step 3: read the timerfd 5 times. Each read blocks until
     * at least one expiration has occurred and returns the count. */
    uint64_t expirations;
    uint64_t total = 0;
    for (int i = 0; i < 5; i++)
    {
        ssize_t n = read(tfd, &expirations, sizeof(expirations));
        if (n != sizeof(expirations))
            die("read timerfd");
        total += expirations;
        printf("  read %d: %llu expiration(s) since last read\n",
               i + 1, (unsigned long long)expirations);
    }

    printf("Total: %llu expirations over ~250 ms\n",
           (unsigned long long)total);

    close(tfd);
    printf("\n");
}

/* ---------- Demo 3: both through epoll ---------- */

static void demo_epoll_both(void)
{
    printf("--- 3. Waiting on both through epoll ---\n");

    /* Block signals for the signalfd */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
        die("sigprocmask");

    /* signalfd for signals */
    int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (sfd == -1) die("signalfd");

    /* timerfd for 30 ms expiry */
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (tfd == -1) die("timerfd_create");

    struct itimerspec ts = { 0 };
    ts.it_value.tv_nsec = 30 * 1000 * 1000;
    if (timerfd_settime(tfd, 0, &ts, NULL) == -1)
        die("timerfd_settime");

    /* Create epoll and register both fds */
    int epfd = epoll_create1(0);
    if (epfd == -1) die("epoll_create1");

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = sfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev) == -1)
        die("epoll_ctl signalfd");

    ev.data.fd = tfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &ev) == -1)
        die("epoll_ctl timerfd");

    printf("Registered both fds with epoll.\n");
    printf("Sending SIGUSR1 to self, waiting for timer...\n");

    /* Send the signal AFTER arming the timer, so we can see
     * both events come through */
    raise(SIGUSR1);

    /* Wait for events */
    struct epoll_event events[4];
    int n = epoll_wait(epfd, events, 4, 1000);
    if (n == -1) die("epoll_wait");

    printf("epoll_wait returned %d ready fd(s):\n", n);

    for (int i = 0; i < n; i++)
    {
        if (events[i].data.fd == sfd)
        {
            struct signalfd_siginfo info;
            read(sfd, &info, sizeof(info));
            printf("  signalfd: SIGUSR1 from pid %u\n", info.ssi_pid);
        }
        else if (events[i].data.fd == tfd)
        {
            uint64_t expirations;
            read(tfd, &expirations, sizeof(expirations));
            printf("  timerfd:  %llu expiration(s)\n",
                   (unsigned long long)expirations);
        }
    }

    close(epfd);
    close(sfd);
    close(tfd);
    printf("\n");
}

/* ---------- Demo 4: periodic tick through epoll ---------- */

static void demo_periodic_tick(void)
{
    printf("--- 4. Periodic tick through epoll ---\n");

    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (tfd == -1) die("timerfd_create");

    /* Repeat every 40 ms, 10 times = 400 ms total */
    struct itimerspec ts = { 0 };
    ts.it_value.tv_nsec    = 40 * 1000 * 1000;
    ts.it_interval.tv_nsec = 40 * 1000 * 1000;
    if (timerfd_settime(tfd, 0, &ts, NULL) == -1)
        die("timerfd_settime");

    int epfd = epoll_create1(0);
    if (epfd == -1) die("epoll_create1");

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = tfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &ev) == -1)
        die("epoll_ctl");

    printf("Waiting for 10 ticks at 40 ms each...\n");

    int ticks = 0;
    while (ticks < 10)
    {
        struct epoll_event events[1];
        int n = epoll_wait(epfd, events, 1, 500);
        if (n == -1) die("epoll_wait");
        if (n == 0) continue;   /* timeout, try again */

        uint64_t expirations;
        read(tfd, &expirations, sizeof(expirations));
        for (uint64_t i = 0; i < expirations && ticks < 10; i++)
        {
            printf("  tick %d\n", ++ticks);
        }
    }

    close(epfd);
    close(tfd);
    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("      SIGNALFD + TIMERFD            \n");
    printf("====================================\n\n");

    demo_signalfd();
    demo_timerfd();
    demo_epoll_both();
    demo_periodic_tick();

    printf("All demos complete.\n");
    return 0;
}

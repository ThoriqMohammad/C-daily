/* epoll_demo.c
 *
 * A tour of epoll: Linux's scalable I/O readiness notification.
 *
 * epoll is what lets a single thread wait on thousands of file
 * descriptors efficiently. Every modern network server — nginx,
 * Redis, HAProxy, Node.js, the Go runtime — uses epoll on Linux.
 *
 * The program walks through:
 *
 *   1. Blocking vs non-blocking reads (why epoll needs O_NONBLOCK)
 *   2. Creating an epoll instance and registering fds
 *   3. Waiting for events with epoll_wait
 *   4. Level-triggered vs edge-triggered semantics
 *   5. Why epoll beats select and poll at scale
 *
 * The demo uses pipes as the event sources because they're
 * self-contained and easy to trigger. The same code paths work
 * for sockets, eventfd, signalfd, timerfd — anything with a file
 * descriptor.
 *
 * Usage:
 *   ./epoll_demo
 *
 * Skills practiced:
 *   - epoll_create1, epoll_ctl, epoll_wait
 *   - struct epoll_event and its packed bitfield
 *   - O_NONBLOCK on file descriptors
 *   - Level-triggered vs edge-triggered events
 *   - struct epoll_event.data.ptr for per-fd context
 *   - Handling EAGAIN / EWOULDBLOCK on read
 *   - Scaling comparison: epoll vs select
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <time.h>

#define MAX_EVENTS 8

/* ---------- helpers ---------- */

/* Set a file descriptor to non-blocking mode.
 * epoll requires this: with level-triggered events, you must
 * drain the fd fully or epoll_wait keeps returning. With
 * edge-triggered events, you must drain until EAGAIN or you
 * miss future notifications. Either way: O_NONBLOCK. */
static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl(F_GETFL)");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        perror("fcntl(F_SETFL)");
}

/* Trigger a short delay via nanosleep. Used to space out writes
 * across the demo so the reader sees events in isolation. */
static void delay_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* ---------- Demo 1: non-blocking read ---------- */

static void demo_nonblocking(void)
{
    int fds[2];
    printf("--- 1. Non-blocking read on a pipe ---\n");

    if (pipe(fds) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    set_nonblocking(fds[0]);

    /* Try to read with nothing available */
    char buf[32];
    ssize_t n = read(fds[0], buf, sizeof(buf));
    if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        printf("read with no data -> EAGAIN (returned immediately)\n");

    /* Write, then read again */
    write(fds[1], "hi", 2);
    n = read(fds[0], buf, sizeof(buf));
    printf("after writing 'hi', read returned %zd bytes: '%.*s'\n",
           n, (int)n, buf);

    close(fds[0]);
    close(fds[1]);
    printf("\n");
}

/* ---------- Demo 2: epoll with multiple pipes ---------- */

/* Context attached to each registered fd via epoll_event.data.ptr */
typedef struct
{
    int fd;
    const char *name;
} source;

static void demo_epoll_basic(void)
{
    printf("--- 2. epoll with three pipes ---\n");

    int epfd = epoll_create1(0);
    if (epfd == -1)
    {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    /* Create three pipes and register the read end of each */
    int pipes[3][2];
    source srcs[3];

    for (int i = 0; i < 3; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        set_nonblocking(pipes[i][0]);

        srcs[i].fd = pipes[i][0];
        srcs[i].name = (i == 0) ? "alpha" : (i == 1) ? "beta" : "gamma";

        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;                /* interest in readability */
        ev.data.ptr = &srcs[i];             /* attach our context */

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, pipes[i][0], &ev) == -1)
        {
            perror("epoll_ctl(ADD)");
            exit(EXIT_FAILURE);
        }
    }

    printf("Registered %d pipes. Writing to alpha, then gamma...\n", 3);

    /* Write to alpha and gamma, but not beta */
    write(pipes[0][1], "from alpha", 10);
    write(pipes[2][1], "from gamma", 10);

    /* Wait for events */
    struct epoll_event events[MAX_EVENTS];
    int n = epoll_wait(epfd, events, MAX_EVENTS, 200 /* ms */);
    printf("epoll_wait returned %d ready fd(s)\n", n);

    for (int i = 0; i < n; i++)
    {
        source *s = (source *)events[i].data.ptr;
        char buf[64];
        ssize_t r = read(s->fd, buf, sizeof(buf));
        if (r > 0)
            printf("  %s: read %zd bytes: '%.*s'\n",
                   s->name, r, (int)r, buf);
    }

    /* Cleanup */
    for (int i = 0; i < 3; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    close(epfd);

    printf("\n");
}

/* ---------- Demo 3: level-triggered vs edge-triggered ---------- */

static void demo_trigger_modes(void)
{
    printf("--- 3. Level-triggered vs edge-triggered ---\n");

    int fds[2];
    if (pipe(fds) == -1) { perror("pipe"); exit(EXIT_FAILURE); }
    set_nonblocking(fds[0]);

    /* --- Level-triggered (default) --- */
    int epfd = epoll_create1(0);
    if (epfd == -1) { perror("epoll_create1"); exit(EXIT_FAILURE); }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;                 /* level-triggered by default */
    ev.data.fd = fds[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, fds[0], &ev);

    write(fds[1], "abc", 3);

    struct epoll_event events[MAX_EVENTS];

    int n1 = epoll_wait(epfd, events, MAX_EVENTS, 100);
    int n2 = epoll_wait(epfd, events, MAX_EVENTS, 100);
    printf("Level-triggered:   first wait = %d, second wait = %d\n",
           n1, n2);
    printf("  (both 1: data is still there, so epoll keeps reporting)\n");

    /* Drain the pipe */
    char buf[16];
    while (read(fds[0], buf, sizeof(buf)) > 0) { /* drain */ }

    int n3 = epoll_wait(epfd, events, MAX_EVENTS, 100);
    printf("  after draining:   third wait = %d\n", n3);

    close(epfd);

    /* --- Edge-triggered --- */
    epfd = epoll_create1(0);
    if (epfd == -1) { perror("epoll_create1"); exit(EXIT_FAILURE); }

    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLET;       /* edge-triggered */
    ev.data.fd = fds[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, fds[0], &ev);

    write(fds[1], "xyz", 3);

    int m1 = epoll_wait(epfd, events, MAX_EVENTS, 100);
    int m2 = epoll_wait(epfd, events, MAX_EVENTS, 100);
    printf("Edge-triggered:    first wait = %d, second wait = %d\n",
           m1, m2);
    printf("  (first 1, second 0: the edge already fired, and we\n");
    printf("   haven't read, so no new edge occurs)\n");

    close(epfd);
    close(fds[0]);
    close(fds[1]);
    printf("\n");
}

/* ---------- Demo 4: scalability ---------- */

static void demo_scalability(void)
{
    printf("--- 4. Why epoll scales ---\n");

    /* We can't actually create thousands of pipes in a demo,
     * but we can describe the algorithm. */

    printf("  select:  O(n) per call — kernel copies the entire fd_set\n");
    printf("           in and out, then scans it. 1000 fds = 1000 checks.\n");
    printf("           Limited to FD_SETSIZE (usually 1024) fds.\n\n");

    printf("  poll:    O(n) per call — passes an array of struct pollfd,\n");
    printf("           kernel scans all of them. No fd limit, but still\n");
    printf("           linear per call.\n\n");

    printf("  epoll:   O(1) per ready fd — kernel keeps an interest list\n");
    printf("           in the epoll instance and a ready list updated by\n");
    printf("           callbacks. epoll_wait returns only the ready fds.\n");
    printf("           10 fds ready out of 10000 = 10 entries, not 10000.\n\n");

    printf("  That's why nginx handles 10000+ connections on one thread\n");
    printf("  while a naive select() server would die at 1024.\n\n");
}

/* ---------- Demo 5: waking from another thread ---------- */

static void demo_self_trigger(void)
{
    printf("--- 5. Triggering epoll from inside the same process ---\n");

    int fds[2];
    if (pipe(fds) == -1) { perror("pipe"); exit(EXIT_FAILURE); }
    set_nonblocking(fds[0]);

    int epfd = epoll_create1(0);
    if (epfd == -1) { perror("epoll_create1"); exit(EXIT_FAILURE); }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = fds[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, fds[0], &ev);

    /* Write, wait, read — full round trip through the event loop */
    printf("Writing a byte to wake epoll...\n");
    write(fds[1], "!", 1);

    struct epoll_event events[MAX_EVENTS];
    int n = epoll_wait(epfd, events, MAX_EVENTS, 500);

    if (n == 1)
    {
        char buf[8];
        read(fds[0], buf, sizeof(buf));
        printf("Woke up, read '%c'. The pattern works.\n", buf[0]);
    }

    close(epfd);
    close(fds[0]);
    close(fds[1]);
    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("             EPOLL DEMO             \n");
    printf("====================================\n\n");

    demo_nonblocking();
    demo_epoll_basic();
    demo_trigger_modes();
    demo_scalability();
    demo_self_trigger();

    printf("Done.\n");
    return 0;
}

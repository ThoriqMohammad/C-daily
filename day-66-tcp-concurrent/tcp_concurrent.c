/* tcp_concurrent.c
 *
 * A TCP echo server that handles many clients at once, using
 * TWO different concurrency models for comparison:
 *
 *   fork model:    one process per client
 *   poll model:    one process, all clients multiplexed
 *
 * Both accept the same kind of connection and echo back the
 * same bytes. They differ in:
 *
 *   - memory:  fork uses N processes; poll uses 1
 *   - CPU:     fork uses more (context switches); poll uses less
 *   - isolation: a crash in one fork child doesn't kill the
 *     others; in the poll model, a crash kills everything
 *   - scaling: fork handles up to ~1000 clients before process
 *     overhead dominates; poll handles ~10,000 before the
 *     O(n) scan per iteration becomes the bottleneck (epoll,
 *     day 51, handles 100,000+ with O(1) per ready fd)
 *
 * Both are legitimate designs, still used in production. Nginx
 * uses the poll/epoll model; Apache prefork uses the fork model;
 * PostgreSQL uses fork; Redis uses poll/epoll. The choice
 * depends on workload and operational constraints.
 *
 * Usage:
 *   ./tcp_concurrent fork [port]    start fork-model server
 *   ./tcp_concurrent poll [port]    start poll-model server
 *   ./tcp_concurrent client [port]  connect and send lines
 *   ./tcp_concurrent test           run self-tests
 *
 * Skills practiced:
 *   - The fork model: accept -> fork -> child handles, parent
 *     loops back to accept more
 *   - SIGCHLD handling to reap exited children (no zombies)
 *   - The poll model: non-blocking fds + poll() in one loop
 *   - struct pollfd and POLLIN / POLLERR
 *   - Accepting and closing connections in a single-threaded
 *     event loop
 *   - Comparing concurrency architectures on paper
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 8080
#define BACKLOG      32
#define BUF_SIZE     1024
#define MAX_CLIENTS  64

/* ---------- helpers ---------- */

static int write_all(int fd, const char *buf, size_t len)
{
    size_t written = 0;
    while (written < len)
    {
        ssize_t n = write(fd, buf + written, len - written);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        written += (size_t)n;
    }
    return 0;
}

/* Set an fd to non-blocking mode */
static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* ---------- setup (shared by both models) ---------- */

static int setup_listen_socket(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("socket");
        return -1;
    }

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                   &yes, sizeof(yes)) == -1)
    {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, BACKLOG) == -1)
    {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

/* ---------- fork model ---------- */

/* Reap any exited children so we don't accumulate zombies.
 * Called from the SIGCHLD handler; WNOHANG means "don't block
 * if no child has exited." */
static void reap_children(int sig)
{
    (void)sig;
    /* Reap all available children — there may be many */
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

/* Handle one client connection in a child process.
 * Reads until EOF, echoes back, then exits. */
static void handle_client_fork(int client_fd,
                               const struct sockaddr_in *client_addr)
{
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr,
              client_ip, sizeof(client_ip));

    printf("[child %d] handling %s:%d\n",
           (int)getpid(), client_ip, ntohs(client_addr->sin_port));
    fflush(stdout);

    char buf[BUF_SIZE];
    for (;;)
    {
        ssize_t n = read(client_fd, buf, sizeof(buf));
        if (n < 0)
        {
            if (errno == EINTR) continue;
            perror("read");
            break;
        }
        if (n == 0) break;   /* client disconnected */

        if (write_all(client_fd, buf, (size_t)n) == -1)
            break;
    }

    close(client_fd);
    printf("[child %d] done\n", (int)getpid());
    fflush(stdout);
    _exit(0);
}

static int run_fork_model(int port)
{
    /* SIGPIPE: writes to a closed socket shouldn't kill us */
    signal(SIGPIPE, SIG_IGN);

    /* SIGCHLD: reap children as they exit, no zombies */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = reap_children;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;   /* restart accept() on signal */
    sigaction(SIGCHLD, &sa, NULL);

    int listen_fd = setup_listen_socket(port);
    if (listen_fd < 0) return 1;

    printf("[parent %d] listening on port %d (fork model)\n",
           (int)getpid(), port);
    printf("[parent %d] press Ctrl+C to stop\n", (int)getpid());
    fflush(stdout);

    for (;;)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(listen_fd,
                               (struct sockaddr *)&client_addr,
                               &addr_len);
        if (client_fd == -1)
        {
            if (errno == EINTR)
                continue;   /* signal interrupted accept; retry */
            perror("accept");
            continue;
        }

        /* Fork a child to handle this client.
         * In the child, close the listen fd (it doesn't need it),
         * handle the client, and exit.
         * In the parent, close the client fd (the child has its
         * own copy), and loop back to accept the next client. */
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid == 0)
        {
            /* Child */
            close(listen_fd);
            handle_client_fork(client_fd, &client_addr);
            /* _exit — not exit — to avoid flushing the parent's
             * stdio buffers a second time. */
            _exit(0);
        }

        /* Parent */
        close(client_fd);
        printf("[parent] spawned child %d\n", (int)pid);
        fflush(stdout);
    }

    close(listen_fd);
    return 0;
}

/* ---------- poll model ---------- */

/* State for one connected client */
typedef struct
{
    int   fd;
    int   in_use;
    char  read_buf[BUF_SIZE];
} client_slot;

static int run_poll_model(int port)
{
    signal(SIGPIPE, SIG_IGN);

    int listen_fd = setup_listen_socket(port);
    if (listen_fd < 0) return 1;

    /* Make the listen socket non-blocking so accept() returns
     * immediately when there's no connection pending. Without
     * this, a spurious poll() wakeup would block on accept. */
    if (set_nonblocking(listen_fd) == -1)
    {
        perror("set_nonblocking");
        close(listen_fd);
        return 1;
    }

    printf("[server %d] listening on port %d (poll model)\n",
           (int)getpid(), port);
    printf("[server %d] press Ctrl+C to stop\n", (int)getpid());
    fflush(stdout);

    client_slot clients[MAX_CLIENTS];
    memset(clients, 0, sizeof(clients));

    /* pollfd array: slot 0 is the listen socket, slots 1..N
     * are the connected clients. */
    struct pollfd pfds[MAX_CLIENTS + 1];

    for (;;)
    {
        /* Build the pollfd array from current state */
        int nfds = 0;

        pfds[nfds].fd = listen_fd;
        pfds[nfds].events = POLLIN;
        pfds[nfds].revents = 0;
        int listen_idx = nfds++;
        (void)listen_idx;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (!clients[i].in_use) continue;
            pfds[nfds].fd = clients[i].fd;
            pfds[nfds].events = POLLIN;
            pfds[nfds].revents = 0;
            nfds++;
        }

        /* Block until at least one fd is ready (or 1 second
         * timeout, so we can respond to signals promptly) */
        int ready = poll(pfds, (nfds_t)nfds, 1000);
        if (ready == -1)
        {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        if (ready == 0) continue;   /* timeout; check signals */

        /* Handle the listen socket first (accept new clients) */
        if (pfds[0].revents & POLLIN)
        {
            for (;;)
            {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(listen_fd,
                                       (struct sockaddr *)&client_addr,
                                       &addr_len);
                if (client_fd == -1)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;   /* no more pending */
                    perror("accept");
                    break;
                }

                /* Find a free slot */
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (!clients[i].in_use) { slot = i; break; }
                }

                if (slot == -1)
                {
                    fprintf(stderr, "too many clients; refusing\n");
                    close(client_fd);
                    continue;
                }

                set_nonblocking(client_fd);
                clients[slot].fd = client_fd;
                clients[slot].in_use = 1;

                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr,
                          ip, sizeof(ip));
                printf("[server] client %s:%d -> slot %d\n",
                       ip, ntohs(client_addr.sin_port), slot);
                fflush(stdout);
            }
        }

        /* Handle connected clients.
         * pfds[1..nfds-1] correspond to the active client slots
         * in the same order they were added. We need to map
         * back to the slot index — rebuild the same iteration. */
        int pfd_idx = 1;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (!clients[i].in_use) continue;

            if (pfd_idx >= nfds) break;   /* shouldn't happen */
            short rev = pfds[pfd_idx].revents;
            pfd_idx++;

            if (rev == 0) continue;

            if (rev & (POLLERR | POLLHUP | POLLNVAL))
            {
                printf("[server] slot %d: hangup\n", i);
                close(clients[i].fd);
                clients[i].in_use = 0;
                continue;
            }

            if (rev & POLLIN)
            {
                ssize_t n = read(clients[i].fd,
                                 clients[i].read_buf, BUF_SIZE);
                if (n < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        continue;   /* spurious wakeup */
                    perror("read");
                    close(clients[i].fd);
                    clients[i].in_use = 0;
                    continue;
                }
                if (n == 0)
                {
                    printf("[server] slot %d: EOF\n", i);
                    close(clients[i].fd);
                    clients[i].in_use = 0;
                    continue;
                }

                /* Echo back */
                if (write_all(clients[i].fd,
                              clients[i].read_buf, (size_t)n) == -1)
                {
                    perror("write");
                    close(clients[i].fd);
                    clients[i].in_use = 0;
                    continue;
                }
            }
        }
    }

    /* Clean up remaining clients on shutdown */
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].in_use) close(clients[i].fd);
    close(listen_fd);
    return 0;
}

/* ---------- client (same as Day 64) ---------- */

static int run_client(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) { perror("socket"); return 1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("connect");
        close(sock);
        return 1;
    }

    const char *lines[] = {
        "hello from the client\n",
        "second line\n",
        "third line\n",
        NULL,
    };

    char buf[BUF_SIZE];

    for (int i = 0; lines[i] != NULL; i++)
    {
        size_t len = strlen(lines[i]);
        write_all(sock, lines[i], len);

        size_t total = 0;
        while (total < len)
        {
            ssize_t n = read(sock, buf + total, sizeof(buf) - total);
            if (n <= 0) goto done;
            total += (size_t)n;
        }
    }

done:
    close(sock);
    printf("[client] done\n");
    return 0;
}

/* ---------- self-test: quick smoke test of poll model ---------- */

/* This is a compile-and-don't-crash check. Actual functional
 * testing requires running the server in another terminal. */
static void test_helpers(void)
{
    printf("Test 1: set_nonblocking on a pipe\n");

    int fds[2];
    if (pipe(fds) == -1) { perror("pipe"); return; }

    if (set_nonblocking(fds[0]) == -1)
    {
        printf("  set_nonblocking failed\n");
        close(fds[0]); close(fds[1]);
        return;
    }

    /* Read on empty non-blocking fd should return EAGAIN */
    char c;
    ssize_t n = read(fds[0], &c, 1);
    int ok = (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
    printf("  non-blocking read returns EAGAIN: %s\n",
           ok ? "ok" : "FAIL");

    close(fds[0]);
    close(fds[1]);
    printf("\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr,
                "Usage:\n"
                "  %s fork [port]     start fork-model server\n"
                "  %s poll [port]     start poll-model server\n"
                "  %s client [port]   connect and send lines\n"
                "  %s test            run helpers self-test\n",
                argv[0], argv[0], argv[0], argv[0]);
        return 1;
    }

    int port = DEFAULT_PORT;
    if (argc >= 3) port = atoi(argv[2]);
    if (port < 1 || port > 65535)
    {
        fprintf(stderr, "Invalid port: %d\n", port);
        return 1;
    }

    if (strcmp(argv[1], "fork") == 0)   return run_fork_model(port);
    if (strcmp(argv[1], "poll") == 0)   return run_poll_model(port);
    if (strcmp(argv[1], "client") == 0) return run_client(port);
    if (strcmp(argv[1], "test") == 0)   { test_helpers(); return 0; }

    fprintf(stderr, "Unknown mode: %s\n", argv[1]);
    return 1;
}

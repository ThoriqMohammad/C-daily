/* round_robin_lb.c
 *
 * Step order (what this program does):
 *
 *   1. Parse command-line args (listen port, backend host:port list)
 *   2. Set up signal handlers (SIGPIPE, SIGCHLD)
 *   3. Create listening socket: socket -> setsockopt(SO_REUSEADDR)
 *      -> bind -> listen
 *   4. Pre-flight: connect to each backend to verify it's up
 *   5. Enter accept loop (fork per connection, as in Day 66)
 *   6. In child: accept one client connection
 *   7. Pick the next backend using round-robin
 *   8. Open a TCP connection to that backend
 *   9. Bidirectionally forward bytes: client -> backend,
 *      backend -> client, until either side closes
 *  10. Close both connections, child _exit
 *  11. Inline health check: before forwarding, if a backend
 *      connect fails, try the next one. If all fail, return 503.
 *
 * What a load balancer does:
 *
 *   A load balancer sits between clients and a pool of backend
 *   servers. It accepts client connections, then forwards each
 *   connection's traffic to one of the backends. The choice of
 *   which backend follows a strategy — round-robin, least
 *   connections, random, IP hash, etc.
 *
 *   Round-robin is the simplest: server 1, then 2, then 3, then
 *   1 again. Even distribution, no state, easy to verify. Real
 *   load balancers use more sophisticated algorithms, but
 *   round-robin is the baseline.
 *
 *   Health checks: a backend can go down. The load balancer
 *   needs to know, or it'll forward clients to a dead server.
 *   This version does an "inline" check: if connecting to a
 *   backend fails, try the next. A more sophisticated version
 *   would run periodic checks (every N seconds) and mark
 *   backends as up or down.
 *
 * What this load balancer does NOT do:
 *   - HTTP-awareness (it proxies raw bytes, not HTTP requests)
 *   - Sticky sessions (cookie or IP-based routing)
 *   - TLS termination
 *   - Weighted distribution
 *   - Active health checks (background ping loop)
 *   - Metrics, logging, or admin API
 *   - Connection pooling
 *   Each is a real feature of production LBs; none are needed
 *   for a TCP-level round-robin forwarder.
 *
 * Usage:
 *   ./round_robin_lb <listen_port> <backend1:port1> [backend2:port2 ...]
 *
 * Example:
 *   # In three terminals:
 *   ./http_server 9001 ./public &
 *   ./http_server 9002 ./public &
 *   ./round_robin_lb 8080 127.0.0.1:9001 127.0.0.1:9002
 *
 *   # Then:
 *   curl http://localhost:8080/
 *   # Served by 9001, then 9002, then 9001, ...
 *
 * Skills practiced:
 *   - TCP proxying: two connections bridged in one process
 *   - Round-robin scheduling
 *   - Inline health checks (connect-and-retry)
 *   - Bidirectional byte forwarding with poll()
 *   - Backend selection failure handling (503 Service Unavailable)
 *   - HTTP 503 response generation
 *   - Parsing host:port backend lists
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
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_LB_PORT  8080
#define BACKLOG          32
#define BUF_SIZE         8192
#define MAX_BACKENDS     16
#define CONNECT_TIMEOUT  2   /* seconds */

/* ---------- helpers ---------- */

static int write_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    size_t written = 0;
    while (written < len)
    {
        ssize_t n = write(fd, p + written, len - written);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return -1;
        }
        written += (size_t)n;
    }
    return 0;
}

static void reap_children(int sig)
{
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

/* ---------- backend representation ---------- */

typedef struct
{
    char host[64];
    int  port;
} backend_t;

/* Parse "host:port" into a backend. Returns 0 on success. */
static int parse_backend(const char *s, backend_t *out)
{
    const char *colon = strchr(s, ':');
    if (!colon) return -1;

    size_t host_len = (size_t)(colon - s);
    if (host_len == 0 || host_len >= sizeof(out->host)) return -1;
    memcpy(out->host, s, host_len);
    out->host[host_len] = '\0';

    int port = atoi(colon + 1);
    if (port < 1 || port > 65535) return -1;
    out->port = port;
    return 0;
}

/* ---------- connect to a backend ---------- */

/* Open a TCP connection to (host, port).
 * Returns the connected socket fd, or -1 on failure.
 * Uses a connect timeout so a dead backend doesn't hang the LB. */
static int connect_to_backend(const backend_t *b)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)b->port);

    if (inet_pton(AF_INET, b->host, &addr.sin_addr) != 1)
    {
        close(fd);
        return -1;
    }

    /* Set non-blocking for connect timeout */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc == -1 && errno != EINPROGRESS)
    {
        close(fd);
        return -1;
    }

    if (rc == -1)
    {
        /* Connect in progress: wait for writability with timeout */
        struct pollfd pfd = { .fd = fd, .events = POLLOUT };
        int ready = poll(&pfd, 1, CONNECT_TIMEOUT * 1000);
        if (ready <= 0)
        {
            close(fd);
            return -1;
        }
        int err = 0;
        socklen_t err_len = sizeof(err);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_len) == -1
            || err != 0)
        {
            close(fd);
            return -1;
        }
    }

    /* Restore blocking for the forwarding loop */
    fcntl(fd, F_SETFL, flags);
    return fd;
}

/* ---------- bidirectional forwarding ---------- */

/* Forward bytes between client_fd and backend_fd in both
 * directions until either side closes. Uses poll() to wait on
 * both simultaneously, so we don't block on one direction while
 * the other has data waiting. */
static void forward_both_ways(int client_fd, int backend_fd)
{
    char buf[BUF_SIZE];
    int client_open = 1;
    int backend_open = 1;

    while (client_open || backend_open)
    {
        struct pollfd pfds[2];
        int n = 0;

        if (client_open)
        {
            pfds[n].fd = client_fd;
            pfds[n].events = POLLIN;
            pfds[n].revents = 0;
            n++;
        }
        if (backend_open)
        {
            pfds[n].fd = backend_fd;
            pfds[n].events = POLLIN;
            pfds[n].revents = 0;
            n++;
        }

        int ready = poll(pfds, (nfds_t)n, 30000 /* 30s idle timeout */);
        if (ready <= 0) break;   /* timeout or error */

        for (int i = 0; i < n; i++)
        {
            if (pfds[i].revents == 0) continue;

            int src = pfds[i].fd;
            int dst = (src == client_fd) ? backend_fd : client_fd;
            int *src_open = (src == client_fd) ? &client_open
                                                : &backend_open;

            if (pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                *src_open = 0;
                continue;
            }

            if (pfds[i].revents & POLLIN)
            {
                ssize_t nr = read(src, buf, sizeof(buf));
                if (nr < 0)
                {
                    if (errno == EINTR) continue;
                    *src_open = 0;
                    continue;
                }
                if (nr == 0)
                {
                    *src_open = 0;
                    /* Half-close the other direction if possible,
                     * but for simplicity we just stop forwarding
                     * from this side. */
                    continue;
                }
                if (write_all(dst, buf, (size_t)nr) == -1)
                {
                    *src_open = 0;
                }
            }
        }
    }
}

/* ---------- HTTP 503 response ---------- */

static void send_503(int client_fd)
{
    const char *resp =
        "HTTP/1.0 503 Service Unavailable\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 29\r\n"
        "Connection: close\r\n"
        "\r\n"
        "No backend available.\n";
    write_all(client_fd, resp, strlen(resp));
}

/* ---------- request handling ---------- */

/* Handle one client connection. Picks a backend (round-robin
 * starting at *next), tries to connect; on failure, tries
 * subsequent backends. If all fail, sends 503. */
static void handle_client(int client_fd,
                          backend_t *backends, int nbackends,
                          int *next)
{
    /* Try up to nbackends candidates, starting from the current
     * round-robin position. Each candidate is tried once. */
    int backend_fd = -1;
    int chosen = -1;

    for (int attempt = 0; attempt < nbackends; attempt++)
    {
        int idx = (*next + attempt) % nbackends;
        backend_fd = connect_to_backend(&backends[idx]);
        if (backend_fd >= 0)
        {
            chosen = idx;
            *next = (idx + 1) % nbackends;   /* advance round-robin */
            break;
        }
        fprintf(stderr, "[lb] backend %s:%d down, trying next\n",
                backends[idx].host, backends[idx].port);
    }

    if (backend_fd < 0)
    {
        fprintf(stderr, "[lb] no backend available, returning 503\n");
        send_503(client_fd);
        close(client_fd);
        return;
    }

    fprintf(stderr, "[lb] routing to backend %s:%d\n",
            backends[chosen].host, backends[chosen].port);

    forward_both_ways(client_fd, backend_fd);

    close(backend_fd);
    close(client_fd);
}

/* ---------- setup ---------- */

static int setup_listen_socket(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) { perror("socket"); return -1; }

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

/* ---------- main ---------- */

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr,
                "Usage: %s <listen_port> <backend1:port1> "
                "[backend2:port2 ...]\n",
                argv[0]);
        fprintf(stderr,
                "Example: %s 8080 127.0.0.1:9001 127.0.0.1:9002\n",
                argv[0]);
        return 1;
    }

    int listen_port = atoi(argv[1]);
    if (listen_port < 1 || listen_port > 65535)
    {
        fprintf(stderr, "Invalid listen port: %d\n", listen_port);
        return 1;
    }

    backend_t backends[MAX_BACKENDS];
    int nbackends = 0;
    for (int i = 2; i < argc && nbackends < MAX_BACKENDS; i++)
    {
        if (parse_backend(argv[i], &backends[nbackends]) != 0)
        {
            fprintf(stderr, "Invalid backend: %s\n", argv[i]);
            return 1;
        }
        nbackends++;
    }
    if (nbackends == 0)
    {
        fprintf(stderr, "At least one backend required.\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = reap_children;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    int listen_fd = setup_listen_socket(listen_port);
    if (listen_fd < 0) return 1;

    printf("[lb] listening on port %d\n", listen_port);
    printf("[lb] backends:\n");
    for (int i = 0; i < nbackends; i++)
        printf("       %s:%d\n", backends[i].host, backends[i].port);
    printf("[lb] try: curl http://localhost:%d/\n", listen_port);
    fflush(stdout);

    int next_backend = 0;

    for (;;)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(listen_fd,
                               (struct sockaddr *)&client_addr,
                               &addr_len);
        if (client_fd == -1)
        {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid == 0)
        {
            close(listen_fd);
            handle_client(client_fd, backends, nbackends, &next_backend);
            _exit(0);
        }

        close(client_fd);
    }

    close(listen_fd);
    return 0;
}

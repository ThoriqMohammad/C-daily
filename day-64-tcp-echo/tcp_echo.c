/* tcp_echo.c
 *
 * A minimal TCP echo server and client in a single binary.
 *
 * The server listens on a port, accepts one connection, and
 * echoes back every line the client sends until the client
 * disconnects. The client connects, sends a few lines, and
 * prints what comes back.
 *
 * This is the "hello, world" of network programming. It teaches:
 *
 *   - socket() creates a file descriptor for network I/O
 *   - bind() attaches a local address (IP + port) to the socket
 *   - listen() marks it as listening for incoming connections
 *   - accept() blocks until a client connects, returns a new fd
 *   - read()/write() work on the accepted fd like any other
 *   - close() tears down the connection
 *
 * The program handles one client at a time. A version that
 * handles many at once is Day 66 (`tcp_concurrent`).
 *
 * Usage:
 *   ./tcp_echo server [port]    start the server (default 8080)
 *   ./tcp_echo client [port]    connect and send a few lines
 *
 * Or in two terminals:
 *   Terminal 1:  ./tcp_echo server
 *   Terminal 2:  ./tcp_echo client
 *
 * Skills practiced:
 *   - socket(), bind(), listen(), accept(), connect()
 *   - struct sockaddr_in and address conversion
 *   - Byte-order handling (htons, htonl)
 *   - read() / write() on network sockets
 *   - Handling EOF (client disconnect)
 *   - SIGPIPE handling on the server side
 *   - Closing file descriptors cleanly
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 8080
#define BACKLOG      1
#define BUF_SIZE     1024

/* ---------- helpers ---------- */

/* Write the whole buffer, handling partial writes */
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

/* ---------- server ---------- */

static int run_server(int port)
{
    /* Ignore SIGPIPE globally. A client that disconnects mid-write
     * would otherwise kill the server with SIGPIPE. Alternative:
     * use send() with MSG_NOSIGNAL on each call. */
    signal(SIGPIPE, SIG_IGN);

    /* 1. Create the socket. AF_INET = IPv4, SOCK_STREAM = TCP. */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        return 1;
    }

    /* SO_REUSEADDR lets us restart the server immediately after
     * shutdown, without waiting for the TIME_WAIT period. */
    int yes = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &yes, sizeof(yes)) == -1)
    {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    /* 2. Bind to the port. INADDR_ANY accepts connections on any
     * local interface (127.0.0.1, LAN, etc.). */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    /* 3. Listen. The kernel will queue up to BACKLOG connections
     * waiting to be accepted. */
    if (listen(server_fd, BACKLOG) == -1)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("[server] listening on port %d\n", port);
    printf("[server] waiting for a client...\n");
    fflush(stdout);

    /* 4. Accept one client. This blocks until a connection
     * arrives. On success it returns a NEW fd for the
     * connection; the original server_fd is still listening. */
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int client_fd = accept(server_fd,
                           (struct sockaddr *)&client_addr,
                           &client_addr_len);
    if (client_fd == -1)
    {
        perror("accept");
        close(server_fd);
        return 1;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr,
              client_ip, sizeof(client_ip));
    printf("[server] client connected from %s:%d\n",
           client_ip, ntohs(client_addr.sin_port));
    fflush(stdout);

    /* 5. Echo loop: read until EOF, write back what we read. */
    char buf[BUF_SIZE];
    for (;;)
    {
        ssize_t n = read(client_fd, buf, sizeof(buf));
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perror("read");
            break;
        }
        if (n == 0)
        {
            /* Client closed its end. */
            printf("[server] client disconnected\n");
            break;
        }

        printf("[server] received %zd bytes, echoing\n", n);

        if (write_all(client_fd, buf, (size_t)n) == -1)
        {
            perror("write");
            break;
        }
    }

    /* 6. Clean up. Close the client fd first, then the server fd. */
    close(client_fd);
    close(server_fd);
    printf("[server] shut down\n");
    return 0;
}

/* ---------- client ---------- */

static int run_client(int port)
{
    /* 1. Create the socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("socket");
        return 1;
    }

    /* 2. Connect to the server. If the server is on the same
     * machine, use 127.0.0.1 (loopback). */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1)
    {
        fprintf(stderr, "inet_pton failed\n");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("connect");
        close(sock);
        return 1;
    }

    printf("[client] connected to 127.0.0.1:%d\n", port);

    /* 3. Send a few lines and read the echoes back */
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
        printf("[client] sending %zu bytes: \"%s\"", len, lines[i]);

        if (write_all(sock, lines[i], len) == -1)
        {
            perror("write");
            break;
        }

        /* Read the echo back. We know exactly how many bytes
         * to expect because the server echoes the same length. */
        size_t total = 0;
        while (total < len)
        {
            ssize_t n = read(sock, buf + total, sizeof(buf) - total);
            if (n <= 0)
            {
                if (n < 0) perror("read");
                else fprintf(stderr, "early EOF\n");
                goto done;
            }
            total += (size_t)n;
        }
        buf[total] = '\0';
        printf("[client] received %zu bytes: \"%s\"", total, buf);
    }

done:
    /* 4. Close. Server sees EOF and shuts down. */
    close(sock);
    printf("[client] done\n");
    return 0;
}

/* ---------- main ---------- */

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr,
                "Usage:\n"
                "  %s server [port]   start the echo server\n"
                "  %s client [port]   connect and send test data\n",
                argv[0], argv[0]);
        return 1;
    }

    int port = DEFAULT_PORT;
    if (argc >= 3)
        port = atoi(argv[2]);
    if (port < 1 || port > 65535)
    {
        fprintf(stderr, "Invalid port: %d\n", port);
        return 1;
    }

    if (strcmp(argv[1], "server") == 0)
        return run_server(port);
    if (strcmp(argv[1], "client") == 0)
        return run_client(port);

    fprintf(stderr, "Unknown mode: %s\n", argv[1]);
    return 1;
}

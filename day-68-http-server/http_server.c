/* http_server.c
 *
 * A small static-file HTTP/1.0 server, built on top of the
 * concurrent TCP server from Day 66.
 *
 * Step order (what this program does):
 *
 *   1. Parse command-line args (port, optional document root)
 *   2. Set up signal handlers (SIGPIPE, SIGCHLD)
 *   3. Create listening socket: socket -> setsockopt(SO_REUSEADDR)
 *      -> bind -> listen
 *   4. Enter accept loop (fork per connection, as in Day 66)
 *   5. In child: read one HTTP request, parse request line
 *   6. Resolve the URI to a filesystem path (with root confinement)
 *   7. Open the file, determine MIME type, stat for Content-Length
 *   8. Write HTTP response: status line, headers, blank line, body
 *   9. Close connection (HTTP/1.0 behavior), child _exit
 *
 * Supported protocol:
 *   - HTTP/1.0 only (one request per connection)
 *   - GET method only (no POST, PUT, etc.)
 *   - Request line: "GET /path HTTP/1.0"
 *   - Status: 200 OK, 400 Bad Request, 404 Not Found, 405 Method
 *     Not Allowed, 501 Not Implemented
 *   - URI "/" maps to "/index.html"
 *   - Path traversal ("..") is rejected
 *   - A small set of MIME types is recognized; everything else
 *     is served as application/octet-stream
 *
 * Why HTTP/1.0 and not 1.1:
 *   1.0 closes the connection after every response. 1.1 keeps
 *   it open (keep-alive), which requires either Content-Length
 *   on every response, chunked transfer encoding, or connection
 *   teardown coordination. For a demo, 1.0's "close on response
 *   done" is simpler and correct. Modern clients accept 1.0
 *   responses fine.
 *
 * What this server does NOT do:
 *   - HTTPS (would need OpenSSL and certificate handling)
 *   - POST/PUT/DELETE
 *   - Range requests
 *   - Conditional requests (If-Modified-Since)
 *   - Compression
 *   - keep-alive / pipelining
 *   - Directory listing
 *   - CGI / dynamic content
 * Each is a real feature; none are needed for a static server.
 *
 * Usage:
 *   ./http_server [port] [docroot]
 *   Default port: 8080
 *   Default docroot: current directory
 *
 *   ./http_server 8000 ./public
 *
 * Then:
 *   curl http://localhost:8080/
 *   curl http://localhost:8080/index.html
 *   curl -I http://localhost:8080/missing
 *   curl -X POST http://localhost:8080/
 *
 * Skills practiced:
 *   - HTTP/1.0 protocol on the wire (parsing and generating)
 *   - Request line parsing, status codes, headers
 *   - MIME type lookup by extension
 *   - Path traversal defense (chroot-like path resolution)
 *   - fork-per-connection server (from Day 66)
 *   - Sending binary files safely over sockets
 *   - Proper Content-Length for binary bodies
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT  8080
#define BACKLOG       32
#define REQ_BUF_SIZE  8192
#define BODY_CHUNK    16384
#define PATH_MAX_LEN  4096

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
            if (errno == EINTR)
                continue;
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

/* ---------- HTTP response construction ---------- */

static const char *status_text(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 501: return "Not Implemented";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

/* Send a complete response with a text/plain body */
static void send_error(int fd, int code, const char *message)
{
    char body[512];
    int body_len = snprintf(body, sizeof(body),
                            "%d %s\n%s\n",
                            code, status_text(code),
                            message ? message : "");

    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.0 %d %s\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        code, status_text(code), body_len);

    write_all(fd, header, (size_t)header_len);
    write_all(fd, body, (size_t)body_len);
}

/* ---------- URI parsing and confinement ---------- */

/* Does the URI contain ".." path components? Rejecting these is
 * one of the most important defenses. "GET /../../../etc/passwd"
 * must NOT escape the document root. */
static int uri_has_traversal(const char *uri)
{
    /* Look for ".." as a full path component */
    const char *p = uri;
    while ((p = strstr(p, "..")) != NULL)
    {
        /* Check the surrounding chars: must be boundary */
        int at_start = (p == uri) || (p[-1] == '/');
        int at_end   = (p[2] == '\0') || (p[2] == '/');
        if (at_start && at_end)
            return 1;
        p += 2;
    }
    return 0;
}

/* Strip query string (everything after '?') from the URI in place */
static void strip_query(char *uri)
{
    char *q = strchr(uri, '?');
    if (q) *q = '\0';
}

/* Build a safe filesystem path:
 *   - reject if URI contains .. traversal
 *   - join docroot + uri
 *   - map "/" to "/index.html"
 * Returns 0 on success, -1 on rejection. */
static int resolve_path(const char *docroot, const char *uri,
                        char *out, size_t out_size)
{
    if (uri_has_traversal(uri))
        return -1;
    if (uri[0] != '/')
        return -1;

    if (strcmp(uri, "/") == 0)
        uri = "/index.html";

    int n = snprintf(out, out_size, "%s%s", docroot, uri);
    if (n < 0 || (size_t)n >= out_size)
        return -1;
    return 0;
}

/* ---------- MIME type lookup ---------- */

static const char *mime_type(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";

    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html";
    if (strcmp(dot, ".css") == 0)  return "text/css";
    if (strcmp(dot, ".js") == 0)   return "application/javascript";
    if (strcmp(dot, ".txt") == 0)  return "text/plain";
    if (strcmp(dot, ".png") == 0)  return "image/png";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)  return "image/gif";
    if (strcmp(dot, ".svg") == 0)  return "image/svg+xml";
    if (strcmp(dot, ".json") == 0) return "application/json";
    if (strcmp(dot, ".pdf") == 0)  return "application/pdf";

    return "application/octet-stream";
}

/* ---------- request handling ---------- */

/* Parse the request line from a buffer. Returns 0 on success,
 * filling `method`, `uri`, and `version`. On failure returns
 * -1 (malformed). Returns -2 if the method is recognized but
 * unsupported (e.g., POST). */
static int parse_request_line(const char *line,
                              char *method, size_t method_size,
                              char *uri, size_t uri_size,
                              char *version, size_t version_size)
{
    /* Expected format: METHOD SP URI SP VERSION CRLF
     * We accept any whitespace as the separator for robustness. */
    const char *p = line;
    size_t i = 0;

    /* Method */
    while (*p && !isspace((unsigned char)*p) && i + 1 < method_size)
        method[i++] = *p++;
    method[i] = '\0';
    if (i == 0) return -1;
    while (*p && isspace((unsigned char)*p)) p++;

    /* URI */
    i = 0;
    while (*p && !isspace((unsigned char)*p) && i + 1 < uri_size)
        uri[i++] = *p++;
    uri[i] = '\0';
    if (i == 0) return -1;
    while (*p && isspace((unsigned char)*p)) p++;

    /* Version */
    i = 0;
    while (*p && !isspace((unsigned char)*p) &&
           *p != '\r' && *p != '\n' && i + 1 < version_size)
        version[i++] = *p++;
    version[i] = '\0';

    if (strcmp(method, "GET") == 0)
        return 0;
    /* Recognized but unsupported methods */
    if (strcmp(method, "POST") == 0 ||
        strcmp(method, "PUT") == 0 ||
        strcmp(method, "DELETE") == 0 ||
        strcmp(method, "HEAD") == 0 ||
        strcmp(method, "OPTIONS") == 0)
        return -2;
    return -1;
}

/* Read one HTTP request (up to the blank line that ends headers).
 * Returns the number of bytes read, or -1 on error, or 0 on EOF
 * with nothing read. */
static ssize_t read_request(int fd, char *buf, size_t buf_size)
{
    size_t total = 0;
    while (total < buf_size - 1)
    {
        ssize_t n = read(fd, buf + total, buf_size - 1 - total);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return (ssize_t)total;
        total += (size_t)n;

        /* Check for the end of the header block (blank line) */
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n") || strstr(buf, "\n\n"))
            return (ssize_t)total;
    }
    /* Buffer full without finding header terminator — treat as
     * bad request. */
    return -1;
}

/* Send a file as the response body, with a 200 OK header. */
static void send_file(int fd, const char *path)
{
    struct stat st;
    if (stat(path, &st) == -1)
    {
        send_error(fd, 404, "Not Found");
        return;
    }
    if (!S_ISREG(st.st_mode))
    {
        send_error(fd, 404, "Not Found");
        return;
    }

    int ffd = open(path, O_RDONLY);
    if (ffd == -1)
    {
        send_error(fd, 404, "Not Found");
        return;
    }

    /* Build and send headers */
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        mime_type(path),
        (long long)st.st_size);

    if (write_all(fd, header, (size_t)header_len) == -1)
    {
        close(ffd);
        return;
    }

    /* Stream the body in fixed-size chunks. We do NOT load the
     * whole file into memory — a server must handle large files
     * without unbounded RAM. */
    char chunk[BODY_CHUNK];
    for (;;)
    {
        ssize_t n = read(ffd, chunk, sizeof(chunk));
        if (n < 0)
        {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        if (write_all(fd, chunk, (size_t)n) == -1) break;
    }

    close(ffd);
}

/* Full request-response cycle for one connection */
static void handle_connection(int fd, const char *docroot)
{
    char buf[REQ_BUF_SIZE];
    ssize_t n = read_request(fd, buf, sizeof(buf));
    if (n <= 0)
        return;

    /* Find the end of the request line (first CRLF or LF) */
    char *line_end = strstr(buf, "\r\n");
    if (!line_end) line_end = strchr(buf, '\n');
    if (!line_end)
    {
        send_error(fd, 400, "Missing request line");
        return;
    }
    *line_end = '\0';

    char method[16], uri[1024], version[16];
    int parse_result = parse_request_line(buf,
                                          method, sizeof(method),
                                          uri,   sizeof(uri),
                                          version, sizeof(version));

    if (parse_result == -2)
    {
        send_error(fd, 405, "Method not supported (only GET)");
        return;
    }
    if (parse_result != 0)
    {
        send_error(fd, 400, "Malformed request line");
        return;
    }

    /* We only support GET and HTTP/1.0 for this demo */
    if (strncmp(version, "HTTP/1.", 7) != 0)
    {
        send_error(fd, 400, "Unsupported HTTP version");
        return;
    }

    strip_query(uri);

    char path[PATH_MAX_LEN];
    if (resolve_path(docroot, uri, path, sizeof(path)) != 0)
    {
        send_error(fd, 404, "Not Found");
        return;
    }

    send_file(fd, path);
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
    int port = DEFAULT_PORT;
    const char *docroot = ".";

    if (argc >= 2) port = atoi(argv[1]);
    if (argc >= 3) docroot = argv[2];

    if (port < 1 || port > 65535)
    {
        fprintf(stderr, "Invalid port: %d\n", port);
        return 1;
    }

    /* Normalize docroot: strip trailing slash to avoid "//" in paths */
    char docroot_buf[PATH_MAX_LEN];
    snprintf(docroot_buf, sizeof(docroot_buf), "%s", docroot);
    size_t dr_len = strlen(docroot_buf);
    while (dr_len > 1 && docroot_buf[dr_len - 1] == '/')
        docroot_buf[--dr_len] = '\0';

    /* Verify docroot is a directory */
    struct stat st;
    if (stat(docroot_buf, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "docroot is not a directory: %s\n", docroot_buf);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = reap_children;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    int listen_fd = setup_listen_socket(port);
    if (listen_fd < 0) return 1;

    printf("[http_server] listening on port %d\n", port);
    printf("[http_server] docroot: %s\n", docroot_buf);
    printf("[http_server] try: curl http://localhost:%d/\n", port);
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

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));

            handle_connection(client_fd, docroot_buf);

            close(client_fd);
            _exit(0);
        }

        close(client_fd);
    }

    close(listen_fd);
    return 0;
}

# Day 68 — http_server

A small static-file HTTP/1.0 server, built on the concurrent
TCP server from Day 66. It parses HTTP requests, serves files
from a document root, and rejects the most common attack vector
(path traversal).

Day 3 of the Cloud & Networking arc.

## What This Server Does (step order)

Every incoming connection follows this sequence:

1. **Parse command-line args** — port, docroot
2. **Set up signal handlers** — ignore SIGPIPE, reap SIGCHLD
3. **Create listening socket** — `socket` → `setsockopt(SO_REUSEADDR)` → `bind` → `listen`
4. **Enter accept loop** — fork per connection (as in Day 66)
5. **In child: read one HTTP request** — up to the blank line
6. **Parse request line** — `GET /path HTTP/1.0`
7. **Resolve URI to filesystem path** — reject `..` traversal, map `/` to `/index.html`
8. **Open file, determine MIME type, stat for Content-Length**
9. **Write HTTP response** — status line, headers, blank line, body
10. **Close connection** — HTTP/1.0 behavior; child exits

The step-order comment is at the top of the source file too, so
anyone reading the code sees the plan before the implementation.

## The HTTP Response Format

Every HTTP response — success or error — has the same shape:

    HTTP/1.0 200 OK\r\n
    Content-Type: text/html\r\n
    Content-Length: 1234\r\n
    Connection: close\r\n
    \r\n
    ...body...

Breaking it down:

- **Status line**: `HTTP/1.0 <code> <reason>`
- **Headers**: one per line, `Name: value`, CRLF-terminated
- **Blank line**: `\r\n` marks the end of headers
- **Body**: the actual content (may be binary)

**CRLF matters.** Real HTTP uses `\r\n`, not `\n`. Some clients
tolerate `\n`, but the spec requires CRLF, and a `\r`-less
response can cause subtle bugs in some clients. This server
uses CRLF consistently.

**Content-Length is required for HTTP/1.0 bodies.** Without it,
the client doesn't know when the body ends. HTTP/1.1 adds
chunked encoding (no `Content-Length` needed), but 1.0 doesn't
have that, so every response carries an explicit length.

## The Request Format

A GET request looks like:

    GET /path/to/file.html HTTP/1.0\r\n
    Host: localhost:8080\r\n
    User-Agent: curl/8.0\r\n
    \r\n

The **request line** is the first line: `METHOD URI VERSION`.
Headers follow, and a blank line ends them.

This server only looks at the request line. It reads until the
blank line but ignores the headers. A real server would parse
`Host`, `User-Agent`, `Range`, `Cookie`, etc. — but for a
static-file server, the request line is enough.

## Step 5: Reading the Request

`read_request()` reads into a buffer until it finds `\r\n\r\n`
or `\n\n` — the blank line that ends the headers. That's the
whole "read one request" logic for HTTP/1.0.

    while (total < buf_size - 1) {
        n = read(fd, buf + total, ...);
        if (n == 0) return total;   /* EOF */
        total += n;
        if (strstr(buf, "\r\n\r\n")) return total;
    }

Two details:

- **We check for both `\r\n\r\n` and `\n\n`.** Some clients
  send `\n` instead of `\r\n` as a line ending. Accepting both
  is lenient but harmless.
- **We cap the total.** A malicious client that sends gigabytes
  without a blank line would exhaust our buffer. The cap is
  `REQ_BUF_SIZE = 8192` — enough for any sane request, small
  enough to not be a DoS vector.

## Step 7: Path Traversal Defense

The classic HTTP server vulnerability:

    GET /../../etc/passwd HTTP/1.0

Without defense, `resolve_path()` would build `./../../etc/passwd`
and happily serve the password file. The fix: **reject any URI
that contains a `..` path component.**

    if (uri_has_traversal(uri))
        return -1;

`uri_has_traversal` looks for `..` where the surrounding
characters are `/` or string boundaries — so `..` alone,
`/..`, `../`, `/../`, etc. are all caught. The string
`foo..bar` is not caught, because `..` there is part of a
filename, not a path component.

This is the minimum viable defense. A more thorough version
would `realpath()` the constructed path and check it starts
with `realpath(docroot)`. Both approaches work; the string
check is simpler and catches the common case.

## Step 8: MIME Type Lookup

The `Content-Type` header tells the browser how to interpret
the body. Getting it wrong can cause the browser to render
HTML as plain text, download images as files, or refuse
scripts.

`mime_type()` maps a small set of extensions:

| Extension | Content-Type |
|-----------|--------------|
| `.html`, `.htm` | `text/html` |
| `.css` | `text/css` |
| `.js` | `application/javascript` |
| `.txt` | `text/plain` |
| `.png` | `image/png` |
| `.jpg`, `.jpeg` | `image/jpeg` |
| `.gif` | `image/gif` |
| `.svg` | `image/svg+xml` |
| `.json` | `application/json` |
| `.pdf` | `application/pdf` |

Everything else falls back to `application/octet-stream`,
which tells the browser "this is binary data, download it."
That's not ideal for unknown text formats, but it's safe.

Real servers read `/etc/mime.types` (or equivalent) which has
hundreds of entries. This server has 10, which covers the
common cases.

## Step 9: Streaming the Body

The tempting implementation:

    read whole file into memory
    write whole buffer to socket

The correct implementation:

    open file
    loop:
        read chunk (16 KB)
        write chunk
        if read returned 0: break

The difference is **memory**. A 1 GB file would require 1 GB
of RAM with the first approach — one client could DoS the
server. The chunked approach uses at most 16 KB per connection,
regardless of file size.

Real servers use `sendfile()` syscall, which does the copy
entirely in the kernel. But `read`+`write` is portable and
correct; the 16 KB buffer is the standard idiom for portable
servers.

## HTTP/1.0 vs 1.1: Why 1.0

HTTP/1.1 added:

- **Keep-alive**: one connection serves multiple requests
- **Chunked transfer**: no need for `Content-Length`
- **Pipelining**: send multiple requests without waiting

Each of these is a real feature, but each requires tracking
state across requests. That's the difference between a demo
and a production server.

For this file, HTTP/1.0's semantics — **one request per
connection, then close** — are simpler and correct. Modern
clients handle 1.0 responses perfectly: the connection
closing tells them the response is done, and they open a
new connection for the next request.

If you wanted to add 1.1, the changes would be:

1. Add `Connection: keep-alive` header
2. Loop on the same connection for multiple requests
3. Handle `Connection: close` from the client

Each is a few lines, but the state management grows.

## Usage

    ./http_server [port] [docroot]

Defaults: port 8080, docroot is the current directory.

    ./http_server                  # port 8080, serve current dir
    ./http_server 8000             # port 8000, serve current dir
    ./http_server 8000 ./public    # port 8000, serve ./public

Then test from another terminal:

    curl http://localhost:8080/
    curl http://localhost:8080/index.html
    curl -I http://localhost:8080/missing
    curl -X POST http://localhost:8080/

Or open `http://localhost:8080/` in a browser.

## Example

Terminal 1:

    $ ./http_server 8080 .
    [http_server] listening on port 8080
    [http_server] docroot: .
    [http_server] try: curl http://localhost:8080/

Terminal 2:

    $ curl http://localhost:8080/
    <!DOCTYPE html>
    <html><head><title>Index</title></head>
    <body><h1>Welcome</h1></body></html>

    $ curl -I http://localhost:8080/nonexistent
    HTTP/1.0 404 Not Found
    Content-Type: text/plain
    Content-Length: 27
    Connection: close

    $ curl -X POST http://localhost:8080/
    HTTP/1.0 405 Method Not Allowed
    Content-Type: text/plain
    Content-Length: 41
    Connection: close

## Build

    gcc -Wall -Wextra -std=c99 -o http_server http_server.c

No external libraries. Just the C standard library and
POSIX sockets.

## Skills Practiced

- **HTTP/1.0 protocol** — request line, status codes, headers
- **CRLF line endings** — the protocol detail that bites
  beginners
- **Content-Length** — how clients know when the body ends
- **Content-Type (MIME)** — how clients interpret the body
- **Path traversal defense** — the #1 web server vulnerability
- **fork-per-connection** — the concurrency model from Day 66
- **Streaming file I/O** — read chunk, write chunk, in a loop
- **Binary-safe sockets** — no assumptions about content
- **Signal handling** — SIGPIPE, SIGCHLD
- **Graceful error responses** — 400, 404, 405 with bodies

## Testing the Errors

The server has multiple error paths. Test them all:

    # 200 OK
    curl -I http://localhost:8080/index.html

    # 404 Not Found
    curl -I http://localhost:8080/missing.html

    # 400 Bad Request (malformed request line)
    printf "GARBAGE\r\n\r\n" | nc 127.0.0.1 8080

    # 405 Method Not Allowed
    curl -X POST http://localhost:8080/

    # 404 for path traversal
    curl "http://localhost:8080/../../etc/passwd"

The last one is the security test. Without the
`uri_has_traversal` check, it would serve `/etc/passwd`.

## Notes

- **No HTTPS.** TLS requires OpenSSL or a similar library and
  ~500 lines of certificate management. Not in scope.
- **No POST/PUT/DELETE.** Only GET is implemented. Adding
  POST would require reading the request body (which comes
  after the headers) and parsing `Content-Length`. That's
  another day.
- **No directory listing.** Requesting `/` without an
  `index.html` returns 404. A real server would generate an
  HTML listing. That's a feature, not a bug.
- **No Range requests.** `curl -r 0-100` gets the whole
  file. Real servers implement byte-range requests for
  video streaming and downloads.
- **No conditional requests.** `If-Modified-Since` and
  `If-None-Match` are ignored. Real servers return 304 Not
  Modified for unchanged files.
- **No compression.** Real servers gzip responses to save
  bandwidth. Out of scope.
- **No keep-alive.** HTTP/1.0 closes after each response.
  See the HTTP/1.0 section above for why.
- **No security headers.** Real servers send `X-Content-Type-Options`,
  `X-Frame-Options`, `Content-Security-Policy`. This is a
  toy server.
- **No access logs.** Every request could be logged to a file
  for observability. That's a real feature, but doesn't
  belong in the minimum HTTP server.
- **No rate limiting.** A client can hammer this server
  without limit. Rate limiting is Day 70-ish territory.
- **The fork model is fine here.** One HTTP request is
  short-lived; the fork overhead is negligible. For
  10,000 concurrent connections, you'd want the poll/epoll
  model from Day 51 or Day 66. See the tradeoff discussion
  in Day 66.
- **`_exit()` not `exit()` in the child.** Same reason as
  Day 66: `exit()` would flush stdio buffers that might
  have been written by the parent, causing duplicate output.
- **No `SIGCHLD` blocking.** Between `fork` and the child
  exiting, the parent's SIGCHLD handler is running.
  There's a theoretical race where the child exits before
  the parent has installed the handler — but the handler
  is installed before `fork`, so this is fine.

## What You Can Extend

- **POST support** — read `Content-Length` from headers,
  then read the body, then handle it
- **Directory listing** — if the path is a directory and
  has no `index.html`, generate an HTML listing
- **Range requests** — parse `Range: bytes=start-end`,
  respond with `206 Partial Content` and just those bytes
- **Conditional requests** — `If-Modified-Since`, `If-None-Match`
- **HTTPS** — OpenSSL, self-signed cert, TLS handshake
- **MIME database** — read `/etc/mime.types` instead of
  hard-coding
- **Access log** — append one line per request to a log file
- **Rate limiting** — count requests per IP per second,
  reject over the limit (preview of Day 70's rate limiter)
- **Epoll model** — replace fork with epoll (Day 51) and
  scale to 100,000 connections
- **Security headers** — add `X-Content-Type-Options: nosniff`
  and `X-Frame-Options: DENY`

## Why This Matters for Systems Programming

- **HTTP is the lingua franca of the internet.** Every web
  app, every REST API, every GraphQL endpoint, every CDN —
  all HTTP. Understanding the wire format makes debugging
  network issues tractable.
- **The server model transfers.** Replace HTTP with any other
  line-oriented protocol (SMTP, FTP, IRC, SSH's initial
  handshake) and the shape is the same: read a request,
  parse it, generate a response, close.
- **The error handling is instructive.** 400, 404, 405 — each
  is a specific failure mode with a specific response.
  Well-designed servers make these paths explicit and test
  them.
- **The path traversal defense is real.** It's CVE-worthy
  material. Serving `/etc/passwd` via a URL is the kind of
  bug that gets companies in the news.
- **The fork-per-connection model** is still used. OpenBSD's
  httpd, some embedded web servers, and the classic Apache
  prefork all use it.

## Why This Matters for Cybersecurity

- **HTTP is the #1 attack surface.** SQL injection, XSS,
  SSRF, CSRF, path traversal, header injection, request
  smuggling — all HTTP-level attacks. Understanding the
  protocol is prerequisite to understanding the attacks.
- **Path traversal is a concrete example.** The defense in
  `uri_has_traversal` is the standard fix, but variations
  exist: URL-encoded `%2e%2e`, double-encoded `%252e`,
  Unicode tricks, symlink traversal. Real defenses need
  `realpath()` + prefix check, plus URL decoding first.
- **The `Content-Length` header can be abused.** Request
  smuggling attacks exploit differences in how servers
  interpret `Content-Length` vs `Transfer-Encoding`. This
  server ignores both, which is safe for GET but would be
  vulnerable if POST were implemented naively.
- **Response headers can leak information.** `Server:`
  headers reveal the software version. `X-Powered-By:`
  reveals frameworks. This server omits both, which is
  best practice.
- **No rate limiting means DoS.** A client can connect,
  send garbage, disconnect, repeat — consuming server
  resources. Rate limiting is essential for production.

## Why This Matters as a Portfolio Piece

- **"I wrote an HTTP server"** is a milestone. It's the
  kind of thing every systems engineer does once, and every
  senior engineer has opinions about.
- **The step-order comment** at the top of the file makes
  the code easy to follow. Anyone reading it can see the
  plan before the implementation — that's the sign of
  intentional code.
- **The completeness of the error handling** shows
  attention to detail. Not just "it serves files" but "it
  handles 404, 405, 400, path traversal, malformed
  requests correctly."
- **The explicit list of what it doesn't do** is more
  credible than if it claimed to do everything. A reader
  trusts a README that says "here are the 10 features
  this is not implementing" more than one that says
  "complete HTTP server."
- **It builds on the arc.** Day 64 → 66 → 68 is a clear
  progression: sockets → concurrency → protocol. Day 70
  (load balancer) and Day 72 (container) complete the
  picture.

## A Note on the Step-Order Comment

Starting with Day 68, the source files include a **step-order
comment** at the top — a numbered list of what the program
does, in order, before the detailed description. This is
useful because:

1. **The plan is visible before the implementation.** A
   reader can skim the steps, then follow them in the code.
2. **It's a self-documenting skeleton.** If a function isn't
   covered by the step list, it's probably doing something
   the reader didn't expect.
3. **It aids maintenance.** When modifying the program, the
   step list is a checklist — did this change affect any
   other step?

This is the pattern from Day 68 onward. Earlier entries
don't have it, but the convention is established now.

## The Cloud & Networking Arc

| Day | Project | Focus |
|-----|---------|-------|
| 64 | `tcp_echo` | sockets, ports, client-server |
| 66 | `tcp_concurrent` | fork + poll, scalable servers |
| **68** | **`http_server`** | **HTTP protocol, static file serving** |
| 70 | `round_robin_lb` | load balancing, health checks |
| 72 | `mini_container` | chroot, unshare, containers |

The progression: a socket that echoes bytes (64) → a socket
that handles many clients (66) → a socket that speaks a real
protocol (68). Next: a server that distributes across many
backends (70), then a program that isolates processes (72).
Each step is a layer of abstraction — and each layer is what
the layer above it is built from.

By Day 72, you'll have built a minimal cloud platform from
scratch: TCP, HTTP, load balancing, and process isolation.
The same pieces a Kubernetes pod runs on.

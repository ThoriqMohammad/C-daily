# Day 64 — tcp_echo

A minimal **TCP echo server and client** in one binary. The
"hello, world" of network programming, and the first entry in
the Cloud & Networking arc.

## What It Does

- **Server mode**: listens on a port, accepts one connection,
  echoes back every byte the client sends, exits when the
  client disconnects.
- **Client mode**: connects to `127.0.0.1:<port>`, sends a
  few lines, reads the echoes back, exits.

The interesting part isn't the echo — it's the syscall sequence
that makes network communication possible. Every server you
will ever write, from a web server to a chat backend to a
database, uses exactly this sequence.

## The Syscall Sequence

**Server side:**

    socket()  ->  bind()  ->  listen()  ->  accept()
                                                  |
                                                  v
                                          read() / write()
                                                  |
                                                  v
                                              close()

**Client side:**

    socket()  ->  connect()  ->  read() / write()  ->  close()

That's it. The whole of TCP on the application side is those
five or six calls. The rest is protocol.

## The Calls, Explained

### `socket(AF_INET, SOCK_STREAM, 0)`

Creates an unbound socket. `AF_INET` = IPv4. `SOCK_STREAM` = TCP
(reliable, ordered, connection-oriented). `0` = default protocol
for this family/type combination.

Returns a file descriptor. From here on, it behaves like any
other fd — you can pass it to `select()`, `poll()`, `epoll()`
(Day 51), `dup2()` (Day 36), etc.

### `bind(fd, addr, addr_len)`

Attaches a local address (IP + port) to the socket. For a
server, you usually bind to `INADDR_ANY` (all interfaces) on
the port you want to listen on.

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

The `htons` / `htonl` calls convert from host byte order to
network byte order. This is where most TCP beginners get
stuck — they use `sin_port = 8080` and get `54320` on the
wire, because x86 is little-endian and the network is
big-endian.

### `listen(fd, backlog)`

Marks the socket as listening for incoming connections. The
`backlog` argument is the maximum number of pending
connections that the kernel will queue up before further
SYN packets are dropped.

For a small server, `backlog = 1` is fine. For production,
`backlog = SOMAXCONN` (usually 4096) or a specific number
your service is designed to handle.

### `accept(fd, addr, addr_len)`

Blocks until a client connects, then returns a **new** file
descriptor for that specific connection. The original `fd`
remains a listening socket; the returned fd is what you use
to `read()` and `write()` with the client.

This is the key insight of the accept model: one listening
socket, many connections, each with its own fd. A server that
handles multiple clients accepts in a loop and keeps track
of all the accepted fds (see Day 66 for the concurrent
version).

### `connect(fd, addr, addr_len)`

Client-side counterpart to `accept`. Blocking call that tries
to establish a TCP connection to the address. Returns 0 on
success.

Under the hood, `connect()` sends a TCP SYN, waits for the
SYN-ACK, sends the ACK — the three-way handshake — before
returning. If the server isn't listening, it returns
`ECONNREFUSED` after the kernel decides the connection is
unreachable (usually ~1 second).

### `read(fd, buf, len)` / `write(fd, buf, len)`

Same `read`/`write` you know from files (Day 12, Day 46). On
a connected socket, they transfer bytes over the network. The
kernel handles TCP's reliability (retransmits, sequence
numbers, flow control) transparently.

**Partial reads and writes are normal.** `read()` may return
fewer bytes than requested; `write()` may send fewer bytes
than you asked. The `write_all` helper in this file loops
until the whole buffer is sent:

    while (written < len) {
        n = write(fd, buf + written, len - written);
        if (n < 0) return -1;
        written += n;
    }

Forgetting this is the #1 cause of "my message got truncated"
bugs in network code.

### `close(fd)`

Standard close. For a socket, it sends a TCP FIN, waits for
the peer's FIN, and then the connection is fully torn down.
The peer's `read()` returns 0 (EOF) once the FIN arrives.

## SIGPIPE

If you `write()` to a socket whose peer has closed, the kernel
sends `SIGPIPE` to your process. **Default behavior: terminate
the process.** That's how an innocent-looking echo server dies
the moment a client disconnects mid-write.

Two fixes:

**Option 1 (used here):** ignore `SIGPIPE` globally.

    signal(SIGPIPE, SIG_IGN);

Then `write()` returns `-1` with `errno == EPIPE` instead of
killing the process. Your normal error handling takes over.

**Option 2:** use `send()` with `MSG_NOSIGNAL` flag on every
write.

    send(fd, buf, len, MSG_NOSIGNAL);

The flag suppresses the signal for that specific call. More
fine-grained, slightly more verbose.

The README's code uses Option 1 because it's simpler and
matches what most tutorials do. Production servers often
use Option 2 because they want signal behavior to be
data-dependent.

## SO_REUSEADDR

After a server exits, the port stays in `TIME_WAIT` state for
~60 seconds. Restarting the server in that window fails with
`EADDRINUSE`.

`setsockopt(SO_REUSEADDR)` tells the kernel to let the server
rebind the port immediately, ignoring the `TIME_WAIT`
connection. It's safe (it doesn't allow conflicting bindings
while the server is running) and every server should set it.

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

Without it, every test-run cycle requires a 60-second wait.
With it, you can iterate.

## Usage

    ./tcp_echo server       # in one terminal (default port 8080)
    ./tcp_echo client       # in another

    ./tcp_echo server 9000  # custom port
    ./tcp_echo client 9000

Or from a single terminal using `&`:

    ./tcp_echo server &
    sleep 0.2
    ./tcp_echo client
    wait

## Example Output

Terminal 1 (server):

    [server] listening on port 8080
    [server] waiting for a client...
    [server] client connected from 127.0.0.1:54211
    [server] received 21 bytes, echoing
    [server] received 12 bytes, echoing
    [server] received 11 bytes, echoing
    [server] client disconnected
    [server] shut down

Terminal 2 (client):

    [client] connected to 127.0.0.1:8080
    [client] sending 21 bytes: "hello from the client
    "
    [client] received 21 bytes: "hello from the client
    "
    [client] sending 12 bytes: "second line
    "
    [client] received 12 bytes: "second line
    "
    [client] sending 11 bytes: "third line
    "
    [client] received 11 bytes: "third line
    "
    [client] done

## Testing with Real Tools

You can test the server with standard Unix tools:

    ./tcp_echo server &
    printf "test message\n" | nc 127.0.0.1 8080
    wait

`nc` (netcat) does exactly what the client mode does, without
needing to write a client program.

Or with `telnet`:

    ./tcp_echo server &
    telnet 127.0.0.1 8080
    # type something, press enter
    # press Ctrl+] then type "quit" to exit
    wait

Or from a script with `curl` — although `curl` sends an HTTP
request, the echo server echoes it back verbatim, so you'd see
the raw HTTP request as the response.

## Build

    gcc -Wall -Wextra -std=c99 -o tcp_echo tcp_echo.c

## Skills Practiced

- **`socket()`, `bind()`, `listen()`, `accept()`, `connect()`**
- **`struct sockaddr_in`** and its fields
- **Byte order** — `htons`, `htonl`, and why they matter
- **Partial read/write** handling with a loop
- **`SO_REUSEADDR`** to avoid `TIME_WAIT` delays
- **SIGPIPE** — the #1 way network demos die
- **`inet_ntop`** for printing client addresses
- **`inet_pton`** for converting IP strings to binary
- **File descriptor lifecycle** — accept returns a new fd

## Why Server-First, Concurrency-Second

This file handles **one client at a time**. While serving a
client, the server is not accepting new connections. A second
client that tries to connect will queue in the kernel's
backlog and be ignored until the first client disconnects.

That's the point. Concurrency is a separate concern, and
mixing the two in one file would make both harder to
understand.

**Day 66** (`tcp_concurrent`) takes the same server and adds
two approaches to handle many clients at once:
- **`fork()`** — one process per client
- **`poll()`** — one process, all clients multiplexed

That progression — "simple, then scalable" — is exactly how
network programming is taught, and how real servers evolve
from toy to production.

## Notes

- **The server exits after one client.** Real servers loop
  on `accept()`. That's Day 66's job.
- **No timeouts.** `accept()` blocks forever if no client
  arrives. Real servers use `SO_RCVTIMEO` or `poll()` with a
  timeout, so they can check for shutdown signals and other
  conditions.
- **`INADDR_ANY` accepts connections from any interface** —
  loopback, LAN, etc. For a local-only demo, bind to
  `INADDR_LOOPBACK` (127.0.0.1) instead.
- **The port number is not a service.** No `/etc/services`
  lookup, no DNS resolution. `8080` is just a number. Real
  applications use `getaddrinfo()` to resolve service names
  like `"http"` to port 80.
- **No IPv6.** `AF_INET` is IPv4. IPv6 uses `AF_INET6` and
  `struct sockaddr_in6` — same calls, different struct.
  Day 66 and beyond stay IPv4 for simplicity.
- **The client connects to 127.0.0.1.** To test across
  machines, replace with the server's IP. To test across the
  internet, you'd need port forwarding or a public IP.
- **Firewall.** If your firewall blocks port 8080, the client
  gets `ECONNREFUSED`. Check with `sudo ss -tlnp | grep 8080`
  or `nc -l 8080` in another terminal.

## What You Can Extend

- **Loop on accept()** — accept clients forever, serving one
  at a time
- **Handle multiple clients** — see Day 66
- **Use `getaddrinfo()`** — resolve hostnames, support IPv6
- **Line-based protocol** — read until `\n`, echo line by line
- **HTTP-aware** — parse the request line, respond with a
  real page (see Day 68)
- **Logging** — log every connection with timestamp and IP
- **Graceful shutdown** — catch SIGINT, close the listening
  fd, drain existing connections

## Why This Matters for Systems Programming

- **Everything on the internet is built on this.** Every web
  request, every database query, every DNS lookup, every SSH
  session — TCP socket. `socket/bind/listen/accept` is the
  foundation.
- **The fd model is unified.** Sockets behave like files
  (Day 12), pipes (Day 46), and epoll-waitable objects
  (Day 51). Once you understand one, the others transfer.
- **Byte-order bugs are real.** I've seen production code
  fail because someone forgot `htons` in one place. The
  server "worked" for months until a new client on a
  big-endian system connected.
- **Partial I/O is the default.** `read`/`write` return
  "some bytes" not "the bytes." Handling partial reads and
  writes correctly is what distinguishes a network program
  from a toy.
- **The `TIME_WAIT` / `SO_REUSEADDR` cycle** is the first
  practical obstacle every network programmer hits.
  Understanding it once saves hours of debugging.

## Why This Matters for Cybersecurity

- **Every penetration test starts with port scanning.** Now
  you know what a port *is* — a bound listening socket.
- **Every server has a socket.** Understanding
  `accept()` is prerequisite to understanding what a
  service is doing when it's exploited.
- **TCP handshake manipulation** (SYN floods, RST injection,
  etc.) is a whole class of attacks. Understanding the
  normal flow is prerequisite.
- **Reversing services** — when you `nc` to a port, you see
  what the service echoes back. The echo server is the
  simplest case.
- **Binding to unintended interfaces** — `INADDR_ANY` on
  a misconfigured server exposes it to the whole network.
  A common misconfiguration bug.

## Why This Matters as a Portfolio Piece

- **"I wrote a network server"** is a meaningful milestone.
  Every systems engineer remembers their first working
  socket server.
- **The code is small** — around 250 lines including the
  client — but it demonstrates the full stack from syscalls
  to byte order to signal handling.
- **It's testable** — `nc`, `telnet`, `curl`, or the
  included client can all interact with it.
- **It's the first entry in a coherent arc** — Day 64, 66,
  68 will build from this. Anyone browsing the repo sees a
  progression, not scattered demos.

## The Cloud & Networking Arc

This is **Day 1 of 5** in the focused arc on the systems
concepts behind cloud infrastructure:

| Day | Project | Focus |
|-----|---------|-------|
| 64 | `tcp_echo` | sockets, ports, client-server |
| 66 | `tcp_concurrent` | fork + poll, scalable servers |
| 68 | `http_server` | HTTP protocol, static file serving |
| 70 | `round_robin_lb` | load balancing, health checks |
| 72 | `mini_container` | chroot, unshare, containers |

Each builds on the last. After Day 72, you've built a working
stack from the lowest syscall up to a containerized
application — the same shape as a Kubernetes pod.

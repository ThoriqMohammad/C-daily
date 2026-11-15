# Day 66 — tcp_concurrent

A TCP echo server that handles many clients at once, implemented
**two ways** for comparison:

- **fork model**: one child process per client
- **poll model**: one process, all clients multiplexed

Same protocol, same CLI, two architectures. This is what
concurrency looks like at the network-server level, and both
approaches are still in production today.

## What Changed from Day 64

Day 64 (`tcp_echo`) handled **one client at a time**. While
serving a client, the server wasn't accepting new connections.
A second client that tried to connect would queue in the
kernel's backlog and wait.

Day 66 handles **many clients at once**. Now the server can
serve Alice and Bob and Carol simultaneously. There's just
more than one way to architect that.

## The fork model

The idea: after `accept()`, `fork()` a child process and let
the child handle the client. The parent loops back to `accept()`
immediately.

    parent:
        listen_fd = socket/bind/listen
        forever:
            client_fd = accept(listen_fd)
            if fork() == 0:
                # child
                close(listen_fd)
                echo_loop(client_fd)
                _exit(0)
            else:
                # parent
                close(client_fd)

That's the whole design. Simple, robust, and used by every
Unix server in the 1980s and 1990s — and still by many today.

**Key details:**

- **Parent closes `client_fd` after forking.** The child
  inherits a copy; the parent doesn't need its own. If the
  parent forgets to close, the file descriptor leaks.
- **Child closes `listen_fd`.** The child doesn't accept
  connections; only the parent does. If the child doesn't
  close its copy, the listening socket stays open in the
  child's address space.
- **SIGCHLD handler reaps children.** Without reaping, exited
  children become zombies (process entries that have exited
  but haven't been `wait()`ed on). The handler calls
  `waitpid(-1, NULL, WNOHANG)` in a loop to reap all
  available children.
- **`SA_RESTART`** on the SIGCHLD action ensures `accept()`
  resumes after the signal instead of returning `EINTR`.
- **`_exit(0)` not `exit(0)`** in the child. `exit()` flushes
  stdio buffers, which the parent may have already flushed.
  `_exit()` skips that and terminates cleanly.

**Tradeoffs of the fork model:**

| | |
|---|---|
| Simple | Yes — easy to reason about |
| Isolation | Each client is a separate process; one crash doesn't affect others |
| Memory | ~8 MB per child (process overhead) |
| CPU | Context switches on every request |
| Scaling | Up to ~1000 clients before process overhead dominates |
| Used by | Apache prefork, PostgreSQL (per-connection), older inetd |

## The poll model

The idea: keep **all clients in one process**, use `poll()`
to wait for readiness on any of them, and handle whichever
fds are ready.

    listen_fd = socket/bind/listen + nonblock
    clients = []
    forever:
        pfds = [listen_fd] + [c.fd for c in clients]
        poll(pfds)
        if listen_fd ready:
            client_fd = accept(listen_fd)   # non-blocking
            clients.append(client_fd)
        for each client ready:
            n = read(client_fd, buf)
            if n > 0: write_all(client_fd, buf)
            if n == 0: remove client

That's the whole design. `poll()` blocks until at least one
fd is ready, so the loop consumes zero CPU while idle.

**Key details:**

- **Listen socket must be non-blocking.** After `poll()`
  reports it ready, there might be zero or more pending
  connections. Without `O_NONBLOCK`, a spurious wakeup would
  block on `accept()` until a client connects — freezing
  the whole event loop.
- **Client sockets must be non-blocking.** Same reason:
  `poll()` says "readable" but the read may return `EAGAIN`
  if the client disconnected between poll returning and our
  read. Handle `EAGAIN` as "try again later."
- **`poll()` timeout.** We pass a 1-second timeout so the
  loop wakes up periodically to check for termination
  signals. Without a timeout, a SIGTERM wouldn't be processed
  until the next client activity.
- **The `pfds` array and `clients` array must stay in sync.**
  `pfds[0]` is the listen fd; `pfds[1..N]` correspond to
  active client slots in iteration order. Every iteration
  of the loop rebuilds `pfds` from `clients`, so no stale
  indices.

**Tradeoffs of the poll model:**

| | |
|---|---|
| Simpler memory | ~2 KB per client (just buffers) |
| No context switches | One process handles all requests |
| No isolation | A crash kills all connections |
| Scaling | ~10,000 clients before `O(n)` scan per iteration |
| Used by | nginx, Apache mpm-event, Redis, most modern servers |

## fork vs. poll: the tradeoff

The two models have complementary strengths.

| | fork | poll |
|---|---|---|
| **Memory per client** | ~8 MB | ~2 KB |
| **CPU per request** | Higher (context switches) | Lower |
| **Isolation** | Strong (separate processes) | None (shared process) |
| **Complexity** | Simpler | More complex (state machine) |
| **Scaling limit** | ~1,000 clients | ~10,000 clients |
| **Programmability** | Any blocking call is fine | Everything must be non-blocking |
| **Debugging** | Standard `gdb`, easy | Trickier (concurrent state) |

**When to use fork:**

- Few clients (database connections, SSH sessions)
- **Complex per-client state** with blocking APIs
  (SSL handshakes, database queries, file I/O)
- Isolation matters (each client's work is untrusted)
- You want to use existing blocking libraries

**When to use poll:**

- Many connections, each doing little work (HTTP)
- Latency matters (avoid context switches)
- Memory is tight
- You're willing to write non-blocking code throughout

The real answer for high-performance servers: **use epoll
(Day 51)**, which is `poll()` with O(1) per ready fd instead
of O(n). It handles 100,000+ connections in one process.
The epoll demo shows the exact same event-loop shape as this
poll model, just with a different readiness call.

Nginx, Redis, Node.js, and most modern servers use epoll (or
kqueue on BSD, IOCP on Windows). But **the shape of the loop
is identical to what you see here** — it's `poll()`, but
scaled.

## Usage

    ./tcp_concurrent fork [port]     start fork-model server
    ./tcp_concurrent poll [port]     start poll-model server
    ./tcp_concurrent client [port]   connect and send test data
    ./tcp_concurrent test            helpers self-test

The port defaults to 8080 for both modes. You can run one
model at a time on a given port; to compare, use different
ports or stop one before starting the other.

## Testing the concurrency

To see that it handles multiple clients simultaneously:

**Terminal 1** — start the server:

    ./tcp_concurrent fork 8080
    # or
    ./tcp_concurrent poll 8080

**Terminal 2, 3, 4** — run clients:

    ./tcp_concurrent client 8080

Each client sends three lines and exits. On the server
terminal you should see:

- **fork model**: multiple child PIDs, each handling its
  client, each exiting when done
- **poll model**: multiple client slots active, logs
  showing connections, EOFs, and echos

**Testing simultaneous activity:**

    # In terminal 1: server
    ./tcp_concurrent fork 8080

    # In terminal 2: a client that stays connected
    (echo "line1"; sleep 5; echo "line2") | nc 127.0.0.1 8080 &

    # In terminal 3: another client, while the first is alive
    ./tcp_concurrent client 8080

Both clients are served at the same time. In the fork model,
you'll see two child PIDs. In the poll model, you'll see two
client slots active. Either way, **the second client doesn't
wait for the first to finish** — that's the point of Day 66.

## Build

    gcc -Wall -Wextra -std=c99 -o tcp_concurrent tcp_concurrent.c

## Skills Practiced

- **`fork()` per client** — the process-per-connection model
- **SIGCHLD handling** — reaping zombies, `waitpid(WNOHANG)`
- **`SA_RESTART`** — making `accept()` restart after signals
- **`poll()`** — multiplexing multiple fds in one loop
- **`O_NONBLOCK`** — why both listen and client fds need it
- **`EAGAIN`/`EWOULDBLOCK`** — non-blocking I/O's "no data" signal
- **`struct pollfd`** — the array of fds and events
- **Event loop architecture** — input, events, state machine
- **Comparing architectures** — same protocol, different shape

## Why Both Approaches in One File

The comparison is the point. If I gave you only the fork
version, you'd learn that "fork per client" is a valid
approach but not see the alternative. If I gave you only the
poll version, you'd see the event loop but not realize why
it's a step up from the simpler process-per-client model.

The file is set up so you can switch modes with one argument:

    ./tcp_concurrent fork 8080
    ./tcp_concurrent poll 8080

Same behavior from the client's perspective. Different
internal shapes. That side-by-side view is worth more than
reading two separate tutorials.

## Notes

- **`poll()` is not `epoll()`.** `poll()` scans all registered
  fds on every call, so it's O(n) per iteration. `epoll`
  (Day 51) is O(ready) — it returns only the fds that are
  ready. For 10,000 connections, `poll()` is ~10,000
  comparisons per loop iteration; `epoll` is ~1 comparison
  per ready fd. That's the difference that makes 100,000-
  connection servers possible.
- **`MAX_CLIENTS = 64`** in the poll model. Real servers use
  tens of thousands. The limit exists because the fixed-size
  array makes the code simpler. Bumping it requires a
  dynamic structure (linked list or resizable array).
- **No timeout handling for slow clients.** A client that
  connects and sends nothing ties up a slot forever. Real
  servers implement idle timeouts with `poll()` timeouts and
  per-client timestamps.
- **No backpressure.** `write_all` blocks until the client
  reads (or the socket buffer fills, then blocks indefinitely).
  A malicious client that never reads would tie up the
  server. Real servers use non-blocking writes and queue
  outgoing data.
- **`SIGPIPE` ignored.** Writes to a closed socket return
  `EPIPE` instead of killing the process. This is the
  simplest correct approach. `MSG_NOSIGNAL` on `send()` is
  a slightly finer-grained alternative.
- **fork() model's `accept()` inherits `SO_REUSEADDR`.** The
  child's `listen_fd` copy is closed immediately. The socket
  itself doesn't conflict because the parent and child share
  the same underlying kernel socket; closing in one process
  doesn't affect the other.
- **Zombie children** are the biggest gotcha in the fork
  model. Without `waitpid()`, each exited child stays as a
  `Z` (zombie) in `ps` until the parent exits. The SIGCHLD
  handler in this code prevents that. If you see zombies
  in `ps`, the handler isn't running or is failing.
- **`_exit` vs `exit` in the child.** `exit` runs `atexit`
  handlers and flushes stdio buffers. If the parent has
  buffered output, `exit` in the child would flush that
  same buffer **a second time**, producing duplicate output.
  `_exit` skips all of that. Always use `_exit` (or
  `_Exit`) after `fork()` when the child is just going to
  terminate.

## What You Can Extend

- **Bump `MAX_CLIENTS`** to 4096 — see how memory scales
- **Add per-client idle timeout** — track last-activity time,
  close after N seconds
- **Bidirectional protocol** — instead of echo, implement
  a simple command (e.g., `STATUS`, `TIME`, `QUIT`)
- **Switch to epoll** — replace `poll()` with `epoll_wait()`
  (see Day 51) and measure throughput
- **Multi-threaded poll** — one thread per `poll()` set,
  accept via `SO_REUSEPORT`
- **Thread-per-client** — another classic model, contrasting
  with fork and poll
- **Backpressure** — queue outgoing data, disable `POLLIN`
  when write buffer is full
- **HTTP-aware** — Day 68 takes this further by parsing
  HTTP requests
- **Compare with `nginx`** — run `nginx` and `tcp_concurrent`
  side by side, hit both with `ab` or `wrk`, compare throughput

## Why This Matters for Systems Programming

- **Every server** you'll build, debug, or operate is one
  of these models (or a variant). Understanding both fork
  and poll makes all of them transparent.
- **The fork model is still used heavily.** PostgreSQL's
  per-connection backend is fork-based. So are many
  services that need strong isolation between clients.
- **The poll model is the shape of the modern internet.**
  nginx, Redis, Node.js — all event loops with non-blocking
  I/O. This is the file that shows you the shape.
- **The transition from poll to epoll (Day 51)** is the
  transition from "thousands of clients" to "hundreds of
  thousands." You've already seen epoll; this file shows
  what epoll replaced.
- **Concurrency bugs are hard.** Both models have subtleties:
  fork's SIGCHLD / zombie discipline, poll's non-blocking
  requirements. Learning these in a small server makes
  them tractable. Learning them in production at 3 AM does
  not.

## Why This Matters for Cybersecurity

- **Every web app firewall, every load balancer, every
  reverse proxy** is one of these models. Understanding
  the shape helps you reason about what they can and can't
  do.
- **Slowloris attacks** exploit servers that don't time out
  slow clients. The poll model here has no timeout for
  idle clients — a classic Slowloris vulnerability. Real
  servers add per-client timeouts to mitigate this.
- **Fork bombs** exploit the fork model's process creation.
  A server that unconditionally forks per connection can
  be exhausted by an attacker who opens many connections.
  Rate limiting (see Day 68's arc sibling) mitigates this.
- **ReDoS in URL parsers** is a specific class of attack
  against servers that use regex. Understanding Day 65's
  regex engine makes the ReDoS vulnerability class concrete.
- **The event loop is where authentication happens.** Every
  login, every token check, every ACL decision runs inside
  the loop you see here.

## Why This Matters as a Portfolio Piece

- **"I wrote a concurrent TCP server, two ways"** is a
  substantial achievement. It demonstrates you understand
  concurrency at the OS level, not just "use async/await."
- **The explicit comparison** — same behavior, different
  internals — is the kind of analysis senior engineers do.
  It shows you can evaluate tradeoffs, not just implement
  one design.
- **The detail on SIGCHLD, `_exit`, `SA_RESTART`, `EAGAIN`**
  is exactly the kind of systems knowledge that separates
  "wrote a demo" from "understands what the demo does."
- **This is the natural setup for Day 68 (HTTP server) and
  Day 70 (load balancer).** The arc has a shape: socket →
  concurrency → protocol → distribution → isolation. Each
  step pays off the last.

## The Cloud & Networking Arc

| Day | Project | Focus |
|-----|---------|-------|
| 64 | `tcp_echo` | sockets, ports, client-server |
| **66** | **`tcp_concurrent`** | **fork + poll, scalable servers** |
| 68 | `http

# Day 51 — epoll_demo

A tour of **epoll**, Linux's scalable I/O readiness notification
mechanism — the foundation of every modern network server.

## What Is epoll?

`epoll` is a system call for waiting on multiple file descriptors
simultaneously. You tell it "I care about readability on these
10,000 sockets," and it tells you which of them are ready — without
you having to scan all 10,000 every time.

The classic alternative is `select(2)` or `poll(2)`:

| | `select` | `poll` | `epoll` |
|---|---|---|---|
| Time per call | O(n) | O(n) | O(ready) |
| fd limit | ~1024 | none | none |
| Kernel work | copy + scan | copy + scan | iterate ready list |
| 100 fds ready of 10,000 | scans 10,000 | scans 10,000 | returns 100 |

That O(ready) vs O(n) difference is why `nginx`, `Redis`,
`HAProxy`, `Node.js`, and the Go runtime all use `epoll` on
Linux. A `select`-based server hits a wall at a few thousand
connections; an `epoll`-based server handles hundreds of
thousands.

## The Three Calls

epoll has exactly three operations:

```c
int epfd = epoll_create1(0);                    /* create the instance    */
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);     /* register interest in fd */
epoll_wait(epfd, events, maxevents, timeout_ms);/* wait for events        */

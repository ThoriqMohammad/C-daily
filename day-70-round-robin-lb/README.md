# Day 70 — round_robin_lb

A TCP-level **load balancer** that forwards client connections
to a rotating pool of backend servers, with inline health
checks.

Day 4 of 5 in the Cloud & Networking arc.

## What a Load Balancer Does

A load balancer sits between clients and a pool of backend
servers. Its job: distribute incoming work across the pool so
that no single backend is overloaded.

Client                  LB                   Backends
  |                      |                    [B1] [B2] [B3]
  |--- connect :8080 --> |                    /    |    \
  |                      |--- forward -----> (round-robin picks one)
  |<--- response ------  |<--- response ---- |

Two jobs, in order:

1. **Forward traffic**: client bytes go to a backend, backend
   bytes go to the client. The LB is transparent.
2. **Distribute load**: pick a backend per connection, in a
   way that spreads work evenly and skips dead servers.

Load balancers come in many flavors:

- **L4 (TCP)**: forward bytes without understanding the protocol
- **L7 (HTTP)**: parse the request, route based on URL or headers
- **Hardware** (F5, Citrix): dedicated devices with line-rate ASICs
- **Software** (HAProxy, nginx, Envoy): general-purpose servers
- **Cloud** (AWS ELB, GCP LB, Azure LB): managed services

This file is an L4 load balancer: it forwards raw TCP bytes
and doesn't parse HTTP. That makes it protocol-agnostic —
it works for HTTP, SSH, SMTP, or anything else that runs over
TCP.

## Step Order

The file starts with a numbered plan:

1. Parse args
2. Set up signal handlers
3. Create listening socket
4. Pre-flight: verify backends reachable
5. Enter accept loop (fork per connection)
6. In child: accept one client
7. Pick next backend (round-robin)
8. Connect to that backend
9. Bidirectionally forward bytes
10. Close both connections
11. Inline health check: if a backend fails, try the next;
    if all fail, return 503

That ordering matters. In particular, the health check is
*inline* (step 11) rather than proactive (a background loop).
See the notes on that below.

## Round-Robin

The simplest distribution strategy: server 1, then 2, then
3, then 1 again. The state is a single integer:

```c
int next_backend = 0;
int idx = next_backend;
next_backend = (next_backend + 1) % nbackends;

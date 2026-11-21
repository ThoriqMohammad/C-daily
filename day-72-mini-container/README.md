# Day 72 — mini_container

A minimal Linux container: `unshare` + `chroot` + `fork`. It
runs a command in isolated namespaces (PID, UTS, mount, IPC,
net) and confined to a separate filesystem root.

Day 5 of 5 — the finale of the Cloud & Networking arc.

## What a Container Is

A container is an **isolated process**. It sees its own
process table, its own hostname, its own network interfaces,
its own mount points, its own IPC objects. To the process
inside, it looks like its own machine.

But there's no VM. No hypervisor, no separate kernel. The
same Linux kernel runs on the host and inside the container.
The isolation is provided by **namespaces** — kernel
features that partition the system's resources.

Containers = namespaces + chroot + cgroups + seccomp +
capabilities + layered filesystems + image distribution.

That's a lot. This file implements only the **namespaces +
chroot** part — the core isolation. Everything else is
layered on top by Docker, containerd, Podman, and Kubernetes.

| Feature | This file | Docker |
|---------|-----------|--------|
| PID namespace | ✅ | ✅ |
| Mount namespace | ✅ | ✅ |
| UTS namespace | ✅ | ✅ |
| Network namespace | ✅ | ✅ |
| IPC namespace | ✅ | ✅ |
| Filesystem confinement (chroot) | ✅ | ✅ (overlayfs) |
| cgroups (resource limits) | ❌ | ✅ |
| seccomp (syscall filtering) | ❌ | ✅ |
| Capabilities dropping | ❌ | ✅ |
| User namespaces | ❌ | ✅ |
| Image/registry | ❌ | ✅ |
| Networking (veth, bridge) | ❌ | ✅ |

So this is "Docker without the layer that lets you `docker pull`,
the layer that limits CPU/memory, the layer that filters
syscalls, the layer that gives the container real network
connectivity, and the layer that drops unnecessary root
privileges." In other words, the foundational 10% that
everything else builds on.

## The Namespaces

**PID namespace** — each namespace has its own numbering. The
first process in a new PID namespace is PID 1 (like init on
the host). It can't see processes outside its namespace, and
those outside can't see it.

**UTS namespace** — the hostname and domain name. Different
namespaces, different `hostname` output.

**Mount namespace** — the filesystem mount table. Mounts inside
the namespace are invisible to other namespaces. This is what
makes chroot + /proc mounting isolated.

**IPC namespace** — System V IPC (message queues, semaphores,
shared memory). POSIX IPC is per-mount-namespace.

**Network namespace** — network interfaces, routing tables,
socket table. A new net namespace starts with only a `lo`
interface; no external connectivity until you add a veth
pair and configure it.

The unshare call that starts everything:

    unshare(CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS |
            CLONE_NEWIPC | CLONE_NEWNET);

After this, the calling process's "view of the system" is
isolated. The next `fork()` gives us a process that's
genuinely PID 1 in its own namespace.

## The chroot

Namespaces isolate kernel-level resources. But the filesystem
is still the host's filesystem — `/etc/passwd` still exists,
just in a new mount namespace. To hide the host's filesystem,
we use **chroot**.

`chroot(newroot)` changes the process's filesystem root to
`newroot`. After that, paths are resolved relative to the new
root. `/etc/passwd` inside the chroot refers to
`newroot/etc/passwd`, not the host's `/etc/passwd`.

    chdir(newroot);          /* move into it first         */
    chroot(newroot);         /* make it the new /          */
    chdir("/");              /* update cwd                 */

The `chdir` before `chroot` is important. `chroot()` doesn't
change the current working directory. If the process was at
`/home/user` before chroot, it's still at the same *inode*
after chroot, which is now *outside* the new root. A process
could then escape the chroot by walking up from the current
directory.

`chdir(newroot)` fixes this: the process is at the new root
before chroot, so after chroot it's at the new root's `/`.

This is the classic **chroot escape** vulnerability. Many
old container systems had it. Modern ones use namespaces +
pivot_root instead of chroot precisely because chroot is
not a strong security boundary on its own.

## Mounting /proc

Inside the container, `/proc` needs to be the container's
own proc filesystem, not the host's. Otherwise `ps` inside
the container would show all host processes (in the new
PID namespace, `ps` reads `/proc`, which is the host's).

    mount("proc", "/proc", "proc", 0, NULL);

This is only safe because we're in a new **mount namespace**.
Without that, the mount would affect the host's `/proc` —
very bad. With it, the mount is local to the container.

The `stat("/proc")` before `mount` is a courtesy: if the
rootfs has no `/proc` directory, `mount` fails, and we
warn rather than abort. Real container runtimes require
`/proc` to exist and fail hard if it doesn't.

## PID 1 Semantics

The first process in a new PID namespace is PID 1. Like
init on the host, PID 1 has special behavior:

- **Signals from inside the namespace** to PID 1 are ignored
  unless PID 1 installs a handler. SIGTERM, SIGINT, etc. are
  blocked by default.
- **When PID 1 exits**, the namespace is torn down and all
  other processes in it are killed.
- **Orphaned processes** are re-parented to PID 1 (not to
  host's init). PID 1 must reap them, or they become zombies.

For a shell inside a container, this means: Ctrl+C doesn't
stop the shell (the shell is PID 1, and PID 1 ignores SIGINT).
That's why production containers use an "init process" like
`tini` or `dumb-init` as PID 1 — it installs signal handlers
and forwards signals to the actual application.

For this demo, running `/bin/sh` means `/bin/sh` is PID 1.
It's usable but quirky: Ctrl+C in the shell doesn't work, and
any zombie it spawns stays as a zombie.

## Usage

    sudo ./mini_container <rootfs> <command> [args...]

Requires root. The kernel checks for CAP_SYS_ADMIN on most of
the operations involved (unshare, chroot, mount).

## Setting Up a Rootfs

The hardest part of using this is having a rootfs with the
binaries you want. The simplest test uses the host's root:

    sudo ./mini_container / /bin/sh

That gives you a shell inside a new PID/mount/etc namespace,
but with the host's filesystem. Not very isolated, but it
proves the namespaces work — try `hostname` (you'll see
"mini-container") and `ps aux` (you'll see only the shell
and ps itself, because they're the only processes in the
new PID namespace).

For a real rootfs, you need to copy the binaries you want
plus their shared libraries:

    # Create the directory structure
    sudo mkdir -p /tmp/miniroot/bin /tmp/miniroot/lib \
                  /tmp/miniroot/lib64 /tmp/miniroot/proc

    # Copy a shell and its dependencies
    sudo cp /bin/sh /tmp/miniroot/bin/
    sudo cp /bin/ls /tmp/miniroot/bin/

    # Copy shared libraries (depends on your system)
    sudo cp -a /lib/x86_64-linux-gnu /tmp/miniroot/lib/ 2>/dev/null || \
    sudo cp -a /lib64/* /tmp/miniroot/lib64/ 2>/dev/null

    # Run
    sudo ./mini_container /tmp/miniroot /bin/sh

If `sh` starts but `ls` complains about missing libraries,
you haven't copied the right `.so` files. Use `ldd /bin/ls`
to see what it needs.

For a proper rootfs, use `debootstrap` (Debian), `dnf --installroot`
(Fedora), or `busybox --install` (a single binary with all
applets).

## What to Observe Inside the Container

Once inside, run:

    hostname
    # mini-container (not the host's hostname)

    ps aux
    # Only your shell and the ps command. Nothing from the host.

    ls /proc
    # The container's own proc entries, not the host's.

    id
    # Still shows you as root (we didn't drop privileges —
    # that would be a cgroups/seccomp feature)

    ip addr
    # Only "lo" - the network namespace has no external
    # connectivity. (If "ip" isn't in the rootfs, this fails;
    # that's expected.)

    exit
    # [mini_container] exited with status 0

## Build

    gcc -Wall -Wextra -std=c99 -o mini_container mini_container.c

## Skills Practiced

- **`unshare()`** — the syscall that creates new namespaces
- **`CLONE_NEWPID`, `CLONE_NEWUTS`, `CLONE_NEWNS`,
  `CLONE_NEWIPC`, `CLONE_NEWNET`** — what each namespace
  isolates
- **`chroot()`** — the filesystem confinement primitive
- **The chroot escape** — why `chdir` before `chroot` matters
- **`mount()` inside a mount namespace** — isolated /proc
- **PID 1 semantics** — the special process at the root of
  a PID namespace
- **`fork` after `unshare(CLONE_NEWPID)`** — why the calling
  process isn't the new PID 1
- **`execvp`** — replacing the container process with the
  user's command
- **`waitpid`** — getting the container's exit status

## The Step-Order Comment, in Detail

The step comment at the top of the file is more than a
comment — it's a plan for understanding the whole thing.
Read in order:

1. **Parse args** — trivial, but establishes intent
2. **Check privileges** — unshare needs CAP_SYS_ADMIN
3. **Unshare** — the core syscall. Five flags, five new
   namespaces.
4. **Fork** — because unshare(CLONE_NEWPID) only affects
   *future* children, not the caller. The child becomes PID 1.
5. **Child: chroot setup** — filesystem confinement
6. **Child: mount /proc** — for `ps` and `/proc` reads
7. **Child: set hostname** — because we're in a UTS
   namespace
8. **Child: exec** — replace the process with the user's
   command
9. **Parent: waitpid** — wait and report

Steps 1-2 are setup. Steps 3-4 are the "create the container
environment" phase. Steps 5-8 are "configure what the
container sees." Step 9 is "clean up."

## The Cloud & Networking Arc, Complete

| Day | Project | Focus |
|-----|---------|-------|
| 64 | `tcp_echo` | sockets, ports, client-server |
| 66 | `tcp_concurrent` | fork + poll, scalable servers |
| 68 | `http_server` | HTTP protocol, static file serving |
| 70 | `round_robin_lb` | load balancing, health checks |
| **72** | **`mini_container`** | **chroot, unshare, containers** |

After Day 72, the arc is complete. The five projects together
form a coherent stack:

- **Networking**: TCP sockets, HTTP, load balancing
- **Concurrency**: fork-per-client, poll event loop
- **Isolation**: namespaces, chroot, mount
- **Real-world connection**: this is exactly the stack
  Kubernetes runs on

A reviewer browsing these five entries sees a narrative:
"sockets → services → distribution → isolation." That's the
systems engineer's view of the cloud. Each piece is small;
together they're a platform.

## Notes

- **Root required.** unshare(CLONE_NEW*) and chroot both
  check for CAP_SYS_ADMIN. Without root, they fail with
  EPERM. A production container runtime uses user
  namespaces to allow unprivileged users, but that
  requires UID mapping and is significantly more complex.
- **No cgroups.** CPU, memory, I/O limits require cgroups.
  Without them, a container can consume all the host's
  resources. Docker uses cgroups extensively.
- **No seccomp.** A container process can make any syscall.
  Real containers use seccomp-bpf to whitelist a small
  set (read, write, mmap, exit, ...) and reject the rest.
  Without seccomp, a compromised container process can
  access every syscall the host kernel offers.
- **No capability dropping.** Root inside the container has
  full root — CAP_NET_ADMIN, CAP_SYS_ADMIN, everything.
  Real containers use `capset` to drop most capabilities,
  keeping only the ones the app needs.
- **No user namespaces.** We're running as real root, not
  mapped to a non-root user via user namespace. Real
  rootless containers map UID 0 inside to UID 100000
  outside; a compromise inside doesn't grant root outside.
- **The net namespace is empty.** Only `lo` exists. Real
  container networking uses a veth pair and a bridge to
  give the container connectivity. This is what makes
  containers "networkable" in Docker/K8s.
- **No filesystem layering.** This file uses a single
  chroot directory. Docker uses overlayfs to layer
  read-only image layers and a writable top layer,
  giving you copy-on-write filesystems.
- **No image format.** Docker images are tar archives of
  layers plus metadata. This file just points at a
  directory. Building and distributing images is what
  most of Docker's codebase is about.
- **The container process is PID 1.** As noted, this means
  signals from inside are blocked. Modern containers use
  an init process (tini, dumb-init) to forward signals and
  reap zombies. Without that, a container running multiple
  processes accumulates zombies.
- **No persistence.** When the container exits, the chroot
  directory is unchanged (we didn't write to it). Real
  containers write to a writable layer that's either
  discarded (ephemeral) or committed (docker commit).
- **`_exit` not `exit` in the child.** Same reason as
  Day 66: avoid flushing the parent's stdio buffers
  twice.
- **No `CAP_SYS_CHROOT` check explicitly.** chroot requires
  it, but we check for euid 0 at the top. A container
  running as non-root with CAP_SYS_CHROOT could chroot
  without being root. Not worth the complexity for a
  demo.

## What You Can Extend

- **cgroup limits** — create a cgroup with `mkdir /sys/fs/cgroup/...`,
  set `memory.max` and `cpu.max`, add the container PID
- **seccomp filter** — load a BPF program that allows only
  the syscalls your app needs
- **Capability dropping** — `capset` to reduce CAP_* after
  fork, before exec
- **User namespaces** — unshare(CLONE_NEWUSER) with UID
  mapping so unprivileged users can run containers
- **veth pair networking** — create a virtual ethernet pair,
  move one end into the container, bridge the other
- **overlayfs rootfs** — mount an overlay with a read-only
  lower and writable upper
- **A container init** — replace /bin/sh with a small init
  that forwards signals and reaps zombies
- **Image builder** — tar up the rootfs, store metadata
- **Compare with `docker run`** — `docker run --rm -it
  --network=none alpine sh` runs a similar container;
  inspect `ls /proc/self/ns/` in both

## Why This Matters for Systems Programming

- **Every modern deployment is containers.** Kubernetes,
  ECS, Cloud Run, Docker Compose — all built on the same
  primitives. Understanding namespaces + chroot is
  prerequisite to understanding the whole ecosystem.
- **The syscalls are not magic.** `unshare`, `chroot`,
  `mount`, `fork` — these are individual kernel features
  with specific effects. Docker is ~1 million lines of Go,
  but the actual isolation is a few hundred lines of C.
- **The step-order matters.** chdir before chroot, mount
  /proc after chroot, unshare before fork. Getting any of
  these out of order produces subtle bugs (chroot escapes,
  wrong proc views, PID 1 confusion). Reading code in
  the order it should execute is a valuable habit.
- **The security model is layered.** Namespaces alone don't
  isolate; chroot alone doesn't isolate; cgroups alone
  don't isolate. Each layer adds a dimension, and defense
  in depth is what makes the whole thing strong.
- **Production is complex.** Docker's ~1M lines exist for
  a reason: image management, networking, cgroups,
  seccomp, capabilities, user namespaces, logging,
  health checks, restart policies. None are in this file.
  The point is to see the 100 lines at the core.

## Why This Matters for Cybersecurity

- **Container escapes** are a real attack class. CVE-2019-5736
  (runc) affected Docker, Kubernetes, containerd, and
  Podman — a compromised container could escape to the host.
  Understanding namespaces is prerequisite to understanding
  escapes.
- **Privileged containers are dangerous.** `--privileged` in
  Docker gives the container full capabilities (no seccomp,
  no capability drop). A container with CAP_SYS_ADMIN can
  do everything this file does — but so can any attacker
  who compromises it.
- **The chroot escape** is a specific, well-known vulnerability.
  Many container "isolations" that used chroot alone were
  escapeable. That's why modern runtimes use pivot_root +
  namespaces, not chroot alone.
- **PID 1 and signal handling** matter for container security.
  A container where PID 1 ignores signals and reaps nothing
  is easier to exploit (unreaped processes accumulate,
  eventually exhausting resources).
- **The isolation boundary is the kernel.** Containers share
  the kernel with the host, so a kernel vulnerability affects
  both. VMs don't share a kernel. That's the tradeoff —
  containers are faster, VMs are stronger isolation. For
  untrusted multi-tenant workloads, VMs (or gVisor-style
  user-space kernels) are used.

## Why This Matters as a Portfolio Piece

- **"I wrote a container"** is a strong statement. It
  demystifies Docker and shows you understand the
  primitives, not just `docker run`.
- **The step-order comment** at the top makes the whole
  thing readable in 10 seconds. That's the sign of
  intentional code.
- **The honesty about what it doesn't do** — no cgroups,
  no seccomp, no capabilities — is more impressive than
  claiming feature parity with Docker. It shows you know
  the landscape.
- **It completes the arc.** TCP → concurrency → HTTP →
  LB → container is a coherent systems story. A reviewer
  sees the arc and knows you can build a stack from
  syscalls up.
- **It pairs with Day 61 (`getpid_tree`).** Both are
  `/proc`-heavy. Day 61 reads other processes' `/proc`
  entries; Day 72 creates a process whose `/proc` is
  isolated. The two entries together form a complete
  view of process introspection.

## The Portfolio Narrative

After Day 72, the repo has:

- **Foundations**: file I/O (Day 12), processes (Day 15),
  signals (Day 35), pipes (Day 46), mmap (Day 41)
- **Networking**: TCP echo (Day 64), concurrent server
  (Day 66), HTTP (Day 68)
- **Distribution**: load balancer (Day 70), routing trie
  (Day 71)
- **Isolation**: container (Day 72)
- **Crypto**: ciphers (Days 07-40), RSA (Day 50), DH (Day 55),
  SHA-256 (Day 57), HMAC (Day 60), padding oracle (Day 69)
- **Algorithms**: FFT (Day 53), Huffman (Day 47), LZ77 (Day 52),
  regex (Day 65), trie (Day 71)
- **Games**: tic-tac-toe (Day 06), 2048 (Day 39), Connect Four
  (Day 44), Snake (Day 49), Asteroids (Day 54), Poker (Day 59),
  Minesweeper solver (Day 67)

That's a **complete systems-engineering curriculum**. No single
entry is trivial; no area is missing. The 72-day streak has
produced a portfolio that covers every major systems and
security topic, with working code for each.

## Acknowledging the Arc

The Cloud & Networking arc (Days 64-72) is complete.
It demonstrated, with working code:

1. **TCP sockets**: the foundation of network programming
2. **Concurrency models**: fork vs poll, and their tradeoffs
3. **HTTP**: the protocol that drives the web
4. **Load balancing**: how services scale horizontally
5. **Data structures for routing**: the trie behind
   every router's routing table
6. **Containerization**: process isolation via namespaces

The arc was interleaved with regular rotation entries
(Days 65, 67, 69, 71) to keep the theme distribution
balanced. After Day 72, the repo has roughly:

- Math: 13
- Game: 13
- Cyber: 13
- Kernel: 16
- Pure C: 17

Kernel is ahead due to the arc, but that's justified —
the arc was Kernel-heavy by nature, and it's now
complete. The regular rotation resumes with Day 73.

That's the story. Not just "72 small programs" but
"72 programs that together form a systems-engineering
education."

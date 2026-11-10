# Day 61 — getpid_tree

Walk `/proc` to enumerate every process on the system and print
the **process tree**. That's how `pstree` works, minus the
colored output.

## What Is /proc?

`/proc` is a **pseudo-filesystem** that exposes kernel state
through the file interface. It doesn't live on disk — the kernel
synthesizes the contents on each read.

Every running process has a directory at `/proc/<pid>/`:

| File | Content |
|------|---------|
| `cmdline` | argv, NUL-separated |
| `comm` | process name (15 chars, no spaces) |
| `status` | key: value lines (Name, PPid, State, Uid, ...) |
| `stat` | one-line summary of process state |
| `cwd` | symlink to working directory |
| `exe` | symlink to executable |
| `fd/` | directory of open file descriptors |
| `maps` | memory mappings |
| `environ` | environment variables |

Plus there are system-wide files: `/proc/meminfo`, `/proc/cpuinfo`,
`/proc/uptime`, `/proc/loadavg`, and so on. `top`, `ps`, `free`,
and `lsof` all work by reading these files.

## How the Tree Is Built

A process tree is simple if you have every process's **PID** and
**parent PID (PPid)**:

1. Scan `/proc` for numeric directory names → list of all PIDs.
2. For each PID, read `/proc/<pid>/status` and extract `PPid`.
3. Build the tree by grouping children under parents.
4. Print recursively from PID 1.

That's the entire algorithm. `pstree` does exactly this, plus
a bit of formatting and process-name cleanup.

## Reading /proc/<pid>/status

The `status` file is a list of `Key: Value` lines:

    Name:   bash
    Umask:  0022
    State:  S (sleeping)
    Tgid:   1234
    Pid:    1234
    PPid:   567
    TracerPid:  0
    Uid:    1000    1000    1000    1000
    Gid:    1000    1000    1000    1000
    ...

`read_status_field` in the code opens the file, scans for the key
followed by a colon, and copies the value. That's the standard
way to parse these files.

## Reading /proc/<pid>/cmdline

`cmdline` contains the process's `argv` as a NUL-separated list:

    "/bin/bash\0-l\0-c\0ls -la\0"

To reconstruct `argv[0]`, read up to the first NUL. Real `ps`
decodes the whole thing; we just take the program name and strip
the path.

**Kernel threads** (kworkers, `kthreadd` children) have **empty**
`cmdline`. They're kernel-space threads, not user processes.
The `comm` field is the only meaningful name for them.

## Reading /proc/<pid>/comm

`comm` is 15 characters of the process name (from the kernel's
`task_struct->comm`). It's shorter than `cmdline` but never empty
and never contains spaces. Real tools use `comm` as a fallback
when `cmdline` is empty, which is what this program does.

## Output

    systemd [1]
    |-- systemd-journald [340]
    |-- systemd-udevd [355]
    |-- cron [892]
    |-- dbus-daemon [905]
    |   \-- dbus-broker-lau [1010]
    |-- NetworkManager [912]
    |-- sshd [1201]
    |   \-- sshd [5432]
    |       \-- bash [5445]
    |           \-- getpid_tree [5498]
    |-- tmux: server [1500]
    |   |-- bash [1501]
    |   |   \-- vim [2100]
    |   |-- bash [1502]
    |   \-- bash [1503]
    |-- systemd-resolved [1120]
    ...

The indentation matters: `|-- ` for intermediate children,
`\-- ` for the last. Lines with children get `|   ` as the
prefix for their subtree.

That's exactly the format that `pstree` produces. It's been
the standard tree rendering for decades, and it generalizes
to any tree structure (file hierarchies, JSON dumps, etc.).

## Usage

    ./getpid_tree

No arguments. Reads `/proc` and prints the tree.

## Build

    gcc -Wall -Wextra -std=c99 -o getpid_tree getpid_tree.c

## Skills Practiced

- **`opendir`/`readdir`** on `/proc` (from Day 34)
- **Reading /proc/<pid>/status** as a key: value file
- **Reading /proc/<pid>/cmdline** as a NUL-separated list
- **Reading /proc/<pid>/comm** as a one-line name
- **Building a tree from flat parent pointers**
- **Recursive tree printing** with the `|--`/`\--` scheme
- **Handling races** — a process can disappear between `readdir` and `status`
- **Fallback chains** — cmdline → comm → "?"

## The Race Condition

A scanning program reads processes **as they are** at some
moment, but the process table is constantly changing. Two kinds
of races:

1. **New process appears** between `readdir` and `status` — readdir
   gives a PID, then that process spawns more children. Fine;
   we just miss them.

2. **Process exits** between `readdir` and `status` — the directory
   name exists in `readdir`'s buffer, but by the time we `open`
   `/proc/<pid>/status`, the process is gone. `fopen` fails with
   `ENOENT`.

The code handles (2) by checking whether `read_status_field`
succeeds; if not, it silently skips that process. That's the
right behavior — a vanished process can't be displayed.

That's also why `ps` sometimes shows fewer processes than
`/proc` has directories: at scan time, some processes existed;
by print time, some are gone.

## Why PID 1 Is Special

PID 1 is the first process the kernel creates, and it stays
running for the life of the system. It's the parent of all
user-space processes (or the grandparent, in the case of
short-lived intermediates).

- On **systemd** systems (most modern Linux), PID 1 is `systemd`.
- On **SysV init** systems, PID 1 is `init`.
- On **containers**, PID 1 is the entrypoint process — often
  `bash`, `sh`, or the application binary itself.

Because PID 1 is by definition **the** root of the process
tree, printing from it shows every process. The `print_tree(1, ...)`
call is the whole point of the program.

**In containers**, PID 1 might not exist from the container's
perspective (its PID namespace assigns its own PID 1 to the
container's init). The fallback in `main` handles this: if we
can't find PID 1, we print all processes whose parent is not
in our snapshot — those are roots.

## Notes

- **`/proc/<pid>/status` may fail for other users' processes**
  depending on `/proc/sys/kernel/yama/ptrace_scope` and the
  process's own permissions. Reading `PPid` is usually allowed
  even for other users' processes, but `cmdline` and `environ`
  may be blocked.
- **Kernel threads appear with empty `cmdline`.** The `comm`
  fallback gives `kworker/0:1`, `ksoftirqd/0`, `migration/0`,
  and similar.
- **The process count depends on timing.** A run may see 150
  processes; the next run may see 152 or 148. That's normal.
- **`getpid_tree` appears in its own tree.** The program is a
  running process, so it scans itself. That's fine — it shows
  up as a leaf under its parent shell.
- **PID reuse** is rare but possible. A process could exit and
  its PID be reassigned between two reads. In practice this
  rarely affects a single scan; if it did, we'd just show the
  wrong name. Real tools check for this by re-reading `stat`.
- **The `MAX_PROCS = 4096` cap** is a safety limit. Real
  systems can have 50,000 processes; this demo handles typical
  desktop systems. Bump it if you need to run on a server.
- **No paging** — the entire snapshot is loaded into memory
  before printing. That's fine for 4096 processes (about
  2 MB) but wouldn't scale to millions.

## What You Can Extend

- **Sort children by start time** — read `/proc/<pid>/stat`
  field 22 (starttime in jiffies) and sort
- **Show memory usage** — read `/proc/<pid>/statm` or `/proc/<pid>/status`
  for RSS, virtual size
- **Show CPU time** — read `/proc/<pid>/stat` for utime/stime
- **Filter by user** — read the `Uid:` field from status, show
  only processes for the current user
- **Filter by name** — a mini `pgrep`
- **Live mode** — refresh every second (like `top`)
- **Full argv** — decode all of `cmdline`, not just `argv[0]`
- **Compare with `pstree`** — your output should match the shape
- **Handle PID namespaces** — inside a container, PIDs are
  container-local; `/proc/1` is the container's init, not the
  host's

## Why This Matters for Linux Kernel Work

- **`/proc` is the primary introspection interface** for the
  kernel. Every tool that shows system state uses it.
- **The `task_struct`** that backs `/proc/<pid>/` is the
  kernel's per-process data structure. Reading `/proc` is
  essentially reading `task_struct` fields.
- **Process hierarchies are fundamental.** PID 1, PPid,
  parent-child relationships — these are baked into the
  kernel's process model, and they matter for signals
  (SIGCHLD delivery), sessions (terminal control), and
  cgroups (resource limits).
- **Every process monitoring tool** — `top`, `htop`, `ps`,
  `pstree`, `pgrep`, `lsof`, `systemd-cgls` — reads `/proc`
  the same way. Understanding this one file makes all of
  them transparent.
- **Container runtimes** manipulate `/proc` to provide
  isolation. A container's `/proc` is namespaced, so it sees
  a subset of processes. Understanding `/proc` is
  prerequisite to understanding containers.

## Why This Matters for Cybersecurity

- **Enumeration** is the first step of any incident response
  or penetration test. `ps`, `pstree`, `lsof` are the
  standard tools, and all are front-ends to `/proc`.
- **Hidden processes** — a process that exists in `/proc` but
  not in `ps` is a red flag (possible rootkit).
- **Command-line inspection** — malware often has empty or
  suspicious `cmdline`. Reading `/proc/<pid>/cmdline` directly
  is a way to see what a process is actually running (its
  `argv`), even when `ps` truncates.
- **Environment inspection** — `/proc/<pid>/environ` reveals
  secrets, tokens, and credentials passed via environment
  variables. Real attackers use this; real defenders need to
  know about it.
- **PID namespace confusion** — in containers, PIDs are
  namespaced. A process at PID 100 inside a container might
  be PID 5000 on the host. Understanding `/proc` namespaces
  is essential for container security.

## Why This Matters as a Portfolio Piece

- **"I wrote a `pstree`"** is a recognizable achievement.
  It's small, it works, and anyone who has used Linux
  immediately understands what it does.
- **The `/proc` reading** demonstrates you understand the
  most important kernel interface that user-space uses.
- **The recursive tree printing** is a mini demonstration
  of a broadly useful algorithm. The `|--`/`\--` format
  appears in file browsers, JSON pretty-printers, and
  org-chart visualizations.
- **The race-handling** shows you're thinking about real-world
  conditions, not just "happy path."
- **It rounds out the Kernel arc** with a genuinely
  informative tool, distinct from the low-level syscall
  demos.

## The Kernel Arc

| Day | Project | Aspect |
|-----|---------|--------|
| 12 | `mini_cat` | File I/O syscalls |
| 15 | `mini_shell` | Process creation |
| 33 | `file_stats` | File metadata |
| 34 | `mini_ls` | Directory reading |
| 35 | `signal_demo` | Signals |
| 36 | `dup2_demo` | File descriptors |
| 41 | `mmap_demo` | Memory mapping |
| 46 | `pipe_demo` | Pipes |
| 51 | `epoll_demo` | I/O multiplexing |
| 56 | `signalfd_timerfd` | Async events as fds |
| 58 | `ptrace_demo` | Process tracing |
| **61** | **`getpid_tree`** | **/proc introspection** |

That's every major interface between user space and the kernel,
ending with the introspection filesystem that connects them all.

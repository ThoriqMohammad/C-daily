# Day 58 — ptrace_demo

**ptrace**: the system call that lets one process observe and
control another. It's the foundation of every debugger, every
syscall tracer, and every dynamic-analysis tool on Linux.

## What ptrace Does

ptrace is a single system call with a request parameter:

    long ptrace(enum __ptrace_request request, pid_t pid,
                void *addr, void *data);

The `request` selects the operation. Common ones:

| Request | Purpose |
|---------|---------|
| `PTRACE_TRACEME` | Child says "trace me" |
| `PTRACE_ATTACH` | Parent takes over an existing process |
| `PTRACE_SETOPTIONS` | Configure tracing behavior |
| `PTRACE_SYSCALL` | Resume tracee, stop at next syscall |
| `PTRACE_CONT` | Resume tracee, stop at next signal |
| `PTRACE_SINGLESTEP` | Execute one instruction |
| `PTRACE_GETREGS` | Read tracee's registers |
| `PTRACE_SETREGS` | Write tracee's registers |
| `PTRACE_PEEKTEXT` | Read tracee's memory |
| `PTRACE_POKETEXT` | Write tracee's memory |

The relationship is always parent-and-child. A process can only
trace its descendants (after `PTRACE_TRACEME`) or processes it
has permission to trace (after `PTRACE_ATTACH`).

## How the Protocol Works

**Child side:**

    ptrace(PTRACE_TRACEME, 0, 0, 0);
    raise(SIGSTOP);          /* or exec() — anything that stops */
    /* normal program continues once parent resumes us */

**Parent side:**

    waitpid(child, &status, 0);       /* wait for initial stop   */
    ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_TRACESYSGOOD);
    while (1) {
        ptrace(PTRACE_SYSCALL, child, 0, 0);  /* resume          */
        waitpid(child, &status, 0);           /* wait for stop   */
        if (WIFEXITED(status)) break;
        /* handle stop: read registers, log, decide what to do */
    }

Each call to `PTRACE_SYSCALL` resumes the child until it either:
- makes a syscall (stops on entry, then again on exit), or
- receives a signal (stops on signal)

The parent alternates between two kinds of stops on each syscall:

    Stop 1: syscall-entry  -> read the syscall number from orig_rax
    Stop 2: syscall-exit   -> read the return value from rax

That's why the loop tracks `in_syscall` — it tells us which
phase we're in.

## The x86-64 Register Layout

Syscall tracing is **architecture-specific**. On x86-64:

| Register | Meaning |
|----------|---------|
| `rax` | syscall number (on entry) / return value (on exit) |
| `orig_rax` | syscall number (preserved across the call) |
| `rdi` | 1st argument |
| `rsi` | 2nd argument |
| `rdx` | 3rd argument |
| `r10` | 4th argument |
| `r8` | 5th argument |
| `r9` | 6th argument |

On ARM64 the numbers are completely different (`x8` for the
syscall number, `x0`-`x5` for args, `x0` for return). This is
why strace has dozens of arch-specific source files.

## Reading Registers

    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, child, 0, &regs);

    long syscall_nr = regs.orig_rax;   /* on entry */
    long ret        = regs.rax;        /* on exit  */

    long arg0       = regs.rdi;
    long arg1       = regs.rsi;
    long arg2       = regs.rdx;

The struct is defined in `<sys/user.h>`. It has every register
of the CPU — general-purpose, control, segment — so you have
full visibility into the tracee's state.

## What This Demo Does

1. Forks a child.
2. Child calls `PTRACE_TRACEME`, then `raise(SIGSTOP)`.
3. Parent waits for the initial stop.
4. Parent enables `PTRACE_O_TRACESYSGOOD` so syscall-stops come
   through as `SIGTRAP | 0x80` (distinguishable from other SIGTRAPs).
5. Parent enters a loop: `PTRACE_SYSCALL` → waitpid → read regs.
6. For each syscall, the parent prints the syscall name and
   return value, once on exit.
7. When the child exits, the loop breaks.

The output is essentially a mini-strace:

    [child] write                = 27
    [child] openat               = 3
    [child] write                = 25
    [child] close                = 0
    [child] write                = 25
    [child] exit_group           = 0

That's each syscall the child made, in order, with its return
value.

## Usage

    ./ptrace_demo

Runs the parent-child protocol and prints the trace.

## Build

    gcc -Wall -Wextra -std=c99 -o ptrace_demo ptrace_demo.c

## Skills Practiced

- **ptrace()** and its request types
- **Parent-child debugging model** — how gdb and strace work
- **`PTRACE_TRACEME`** and the initial SIGSTOP
- **`PTRACE_SETOPTIONS`** with `PTRACE_O_TRACESYSGOOD`
- **`PTRACE_SYSCALL`** — the syscall-stop enable
- **`PTRACE_GETREGS`** and `struct user_regs_struct`
- **Identifying syscalls by number** and reading the return value
- **`waitpid()` with `WIFSTOPPED`** and `WIFEXITED`

## Why This Matters for Linux Kernel Work

- **ptrace is implemented in `kernel/ptrace.c`.** It's a
  readable file that shows how the kernel enforces the
  parent-child relationship.
- **Every debugger sits on ptrace.** gdb uses it for
  breakpoints, single-stepping, and memory inspection.
  lldb, rr, and every IDE debugger do the same.
- **strace is literally this demo with more decoding.** If
  you can read this file, you can read strace's source.
- **Container security.** `docker run --cap-add SYS_PTRACE`
  is a real flag because ptrace is a powerful capability
  that most containers don't need.
- **Anti-debugging.** Malware often checks `PTRACE_TRACEME`
  at startup; if it fails with `EPERM`, a debugger is
  attached, and the malware behaves differently.
- **Dynamic analysis.** Tools like `perf`, `ltrace`, `valgrind`,
  and every fuzzer use ptrace or its successor mechanisms
  (`perf_event_open`, `seccomp-bpf`).

## Why This Matters for Cybersecurity

- **Reverse engineering** starts with ptrace. You attach to
  a binary, step through it, inspect memory, and understand
  what it does.
- **Malware analysis** is done in debuggers and tracers.
  Understanding ptrace is the first step toward analyzing
  hostile code safely.
- **Anti-debugging techniques** all exploit ptrace quirks:
  - `PTRACE_TRACEME` returning `EPERM` (already traced)
  - Detecting breakpoint instructions in memory
  - Timing checks (single-stepping is slow)
  - `/proc/self/status` TracerPid field
- **The TracerPid field** is a specific detail: `/proc/PID/status`
  exposes the tracer's PID. It's a documented way for a
  program to detect whether it's being traced.

## Notes

- **This is x86-64 specific.** On ARM, MIPS, or other
  architectures, the register layout is different, and
  `orig_rax`/`rax` would be different fields. The algorithm
  is the same; only the struct member names change.
- **`PTRACE_O_TRACESYSGOOD`** makes syscall-stops report as
  `SIGTRAP | 0x80`. Without it, they come through as plain
  `SIGTRAP`, indistinguishable from a real breakpoint.
- **`orig_rax` is the syscall number on entry AND exit.** The
  kernel preserves it across the syscall so ptrace can
  identify the syscall even on exit. `rax` changes (it's
  overwritten with the return value).
- **We don't actually exec().** The child runs `child_main()`
  directly, which is why we see syscalls from the C library
  too (like `brk` and `arch_prctl`). A real strace would exec
  a fresh binary and only see that binary's syscalls — but
  the C runtime's startup syscalls would still appear.
- **The parent doesn't handle all signals.** In a real tracer,
  each stop needs a decision: deliver the signal to the child,
  suppress it, redirect it, or stop tracing. This demo just
  passes signals through with `PTRACE_SYSCALL` and the signal
  number.
- **No `PTRACE_ATTACH` demo.** The demo uses `TRACEME` because
  it's simpler. `ATTACH` is used by debuggers to take over an
  existing process; it requires the same permissions plus
  additional checks (Yama ptrace_scope, etc.).
- **Performance.** ptrace is slow — every syscall has two
  context switches to the tracer. That's why strace on a
  busy program produces a huge slowdown, and why perf uses
  a different mechanism (`perf_event_open`).

## What You Can Extend

- **Trace a real binary** — `execl("/bin/ls", "ls", "-l", NULL)`
  and watch every syscall `ls` makes
- **Decode arguments** — read `rdi`, `rsi`, `rdx` and print the
  filename for `openat`, the buffer for `write`, etc.
- **Time each syscall** — measure elapsed time between entry
  and exit for profiling
- **Filter by syscall** — only print `write` syscalls
- **`PTRACE_ATTACH`** — trace an already-running process
- **`PTRACE_SINGLESTEP`** — execute one instruction at a time
  (a mini-debugger)
- **Count syscalls** — end with "3 writes, 1 open, 1 close"
- **PTRACE_PEEKUSER** — inspect the child's memory at
  syscall-stop, to read the actual string argument
- **Compare with strace** — `strace -c ./some_program` shows
  the same information in aggregate form

## Why This Matters as a Portfolio Piece

- **"I wrote a tracer"** is one of the classics. Anyone in
  systems or security immediately recognizes it as a
  non-trivial achievement.
- **The debugging model** is foundational. Understanding how
  gdb works is prerequisite to understanding how to build
  tools, not just use them.
- **The x86-64 register layout** is directly relevant to
  systems interviews and security work. Anyone writing
  exploits, doing reverse engineering, or analyzing malware
  needs to know it.
- **The security implications** (anti-debugging, TracerPid,
  container capabilities) show you think beyond "does it
  work" to "what does it mean."

## The Kernel Arc

The Kernel-theme entries now tell a coherent story:

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
| **58** | **`ptrace_demo`** | **Process tracing** |

That's every major Unix/POSIX interface — and ptrace is
arguably the most powerful of them all.

# Day 35 — signal_demo

A tour of **POSIX signals** on Linux.

## What Is a Signal?

A signal is a small asynchronous notification the kernel delivers
to a process. Ctrl+C, a crash, a timer, or another process can
all trigger one.

| Signal | Number | Default Action | Typical Cause |
|--------|--------|----------------|---------------|
| `SIGINT` | 2 | Terminate | Ctrl+C |
| `SIGKILL` | 9 | Terminate (uncatchable) | `kill -9` |
| `SIGUSR1` | 10 | Terminate | User-defined |
| `SIGSEGV` | 11 | Core dump | Bad memory access |
| `SIGALRM` | 14 | Terminate | `alarm()` timer |
| `SIGTERM` | 15 | Terminate | Polite `kill` |
| `SIGSTOP` | 19 | Stop (uncatchable) | Job control |

Numbers shown are for x86 Linux.

## What the Program Shows

| Demo | Idea | Key Calls |
|------|------|-----------|
| 1. Catch | Run a handler when a signal arrives | `sigaction`, `raise` |
| 2. Block | Hold a signal back, then release it | `sigprocmask`, `sigpending` |
| 3. Kill child | Send a signal to another process | `fork`, `kill`, `waitpid` |
| 4. Timer | Wait for a signal without a race | `alarm`, `sigsuspend` |
| 5. Uncatchable | `SIGKILL` and `SIGSTOP` are reserved | `sigaction` fails |

## Usage

    ./signal_demo

## Build

    gcc -Wall -Wextra -std=c99 -o signal_demo signal_demo.c

Linux only. The file defines `_POSIX_C_SOURCE` to expose the POSIX
signal API under `-std=c99`.

## Skills Practiced

- **Signals** — `sigaction`, `raise`, `kill`, `alarm`
- **Signal masks** — `sigprocmask`, `sigpending`, `sigsuspend`
- **Processes** — `fork`, `waitpid`
- **volatile sig_atomic_t** — safe flags shared with handlers
- **Error handling** — checking every system call

## Why `sigaction` and Not `signal`?

| | `signal()` | `sigaction()` |
|---|-----------|---------------|
| Behavior across systems | Varies | Well defined |
| Handler stays installed | Not guaranteed | Yes |
| Control over blocked signals | None | Full |

**Use `sigaction` in new code.**

## Signals Are Not Queued

When a signal is blocked and sent twice, only one is delivered.

| Step | Pending | Handler Runs |
|------|---------|--------------|
| Block `SIGUSR1` | no | 0 |
| Send twice | yes (one bit) | 0 |
| Unblock | no | 1 |

The kernel keeps one bit per standard signal, not a counter.

## Why Handlers Must Be Tiny

A handler can interrupt your program at any instruction,
even in the middle of `printf` or `malloc`.

| Safe in a handler | Unsafe in a handler |
|-------------------|---------------------|
| Set a `volatile sig_atomic_t` flag | `printf`, `malloc`, `free` |
| `write`, `_exit` | Anything that takes a lock |

**Set a flag, return, and let `main` do the real work.**

## Notes

- Handlers only set flags.
- `sigsuspend` unblocks and sleeps in one atomic step.
- `pause()` alone can miss a signal that arrives just before it.
- The child in demo 3 is killed by the default `SIGTERM` action.
- `strsignal` turns a signal number into text.

## What You Can Extend

- Graceful shutdown on `SIGINT` and `SIGTERM`
- `SIGCHLD` handler to reap children
- Real-time signals (`sigqueue`) that do queue
- `signalfd` for signals as file descriptors
- `SA_SIGINFO` to see who sent the signal

  | 35  | signal_demo | 🐧 Linux Kernel | Signals, sigaction, signal masks |

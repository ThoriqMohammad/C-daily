# Day 36 — dup2_demo

A tour of **file descriptor duplication** with `dup()` and `dup2()`—
the syscalls behind shell redirection (`>`, `2>&1`, and friends).

## What Is a File Descriptor?

A file descriptor is a small non-negative integer that names an
open file within a process. Every process starts with three:

| FD | Name | Default Destination |
|----|------|---------------------|
| 0 | `stdin` | the keyboard |
| 1 | `stdout` | the terminal |
| 2 | `stderr` | the terminal |

"Open a file" means the kernel hands you the *next free* number.
"Close a file" means it becomes free again.

## What the Program Shows

| Demo | Idea | Key Calls |
|------|------|-----------|
| 1. `dup()` | Two fds, same open file | `dup`, `write` |
| 2. Redirect | Send `stdout` to a file | `open`, `dup2`, `fflush` |
| 3. Restore | Save fd 1, swap, swap back | `dup`, `dup2` |
| 4. Merge | Send `stderr` where `stdout` goes | `dup2` |

## `dup` vs `dup2`

| | `dup(fd)` | `dup2(oldfd, newfd)` |
|---|-----------|----------------------|
| Returns | the next free fd | `newfd` itself |
| Closes `newfd` first | n/a | yes, silently |
| Typical use | save a descriptor | redirect a descriptor |

`dup2` is the one you will see in real code: it forces a specific
number to point at a specific open file.

## Usage

    ./dup2_demo

Writes a small file, `dup2_demo_out.txt`, as a side effect.

## Build

    gcc -Wall -Wextra -std=c99 -o dup2_demo dup2_demo.c

## Skills Practiced

- **File descriptors** as small integers
- **`dup` and `dup2`** — copying and forcing fd numbers
- **`fflush`** before swapping `stdout` (avoiding mixed output)
- **`open`** with `O_WRONLY | O_CREAT | O_TRUNC`
- **Saving and restoring** fd 1 safely
- **Error handling** — every call checked

## Why `fflush` Before `dup2`?

`printf` buffers its output. Without an `fflush(stdout)` right
before redirecting, text you thought went to the terminal can
be flushed **after** the swap and land in the file.

| Order | Where earlier `printf`s land |
|-------|------------------------------|
| `dup2` then `fflush` | possibly the file |
| `fflush` then `dup2` | the terminal (as intended) |

This is a classic source of "why is my log file out of order?"
bugs. Flush first.

## Why `dup2` Closes `newfd` Silently

`dup2(oldfd, newfd)` will close `newfd` first if it is open,
without asking. That means you should never call
`dup2(fd, STDOUT_FILENO)` unless you either (a) already saved
`STDOUT_FILENO` with `dup`, or (b) genuinely want to lose it.

The save-and-restore pattern used in demo 3 is the safe form:

    int saved = dup(STDOUT_FILENO);   /* remember fd 1     */
    /* ... redirect ... */
    dup2(saved, STDOUT_FILENO);       /* put it back       */
    close(saved);

## The Shell Connection

Shell redirection is literally these syscalls:

| Shell syntax | What happens |
|--------------|--------------|
| `cmd > file` | `open(file); dup2(fd, 1); exec(cmd)` |
| `cmd 2> file` | `open(file); dup2(fd, 2); exec(cmd)` |
| `cmd 2>&1` | `dup2(1, 2); exec(cmd)` |
| `cmd \| cmd` | `pipe(); dup2 into 0 and 1; exec` |

Pipelines are the same idea with a different source fd.

## Notes

- The program **creates** `dup2_demo_out.txt` in the current
  directory; remove it when done.
- Demo 4 leaves stderr pointing at stdout for the rest of the
  process. That is the point—but don't do it in real programs
  unless you mean it.
- `write` is used in demo 1 to avoid mixing `FILE *` buffering
  with raw fd output.
- File descriptor limits (`ulimit -n`) are the reason servers
  call `close` religiously.

## What You Can Extend

- Implement a mini `2>&1` syntax parser
- Build a two-command pipeline with `pipe()` and `fork()`
- Redirect both stdout and stderr to the same file
- Use `fcntl(F_DUPFD, n)` to pick a minimum fd number
- Compare `dup` with opening the same path twice — not the same

## Why This Matters for Linux Kernel Work

- The **file descriptor table** is a per-process kernel structure;
  `dup` and `dup2` manipulate it directly.
- `fork` copies the table, `exec` keeps it — which is why shell
  redirection survives across `exec`.
- Pipes, sockets, and terminals all surface to user space as
  descriptors; understanding them is the base of all of it.
- Almost every real bug in daemons and servers—leaked fds,
  closed-but-still-referenced fds, misordered flushes—traces
  back to this exact area.

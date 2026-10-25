# Day 46 — pipe_demo

A tour of **anonymous pipes** with `pipe()`, `fork()`, and
`dup2()`—the mechanism behind the shell's `|` operator.

## What Is a Pipe?

A pipe is a one-way byte channel provided by the kernel. It has
two ends, each represented by a file descriptor:

    int fds[2];
    pipe(fds);
    /* fds[0] = read end */
    /* fds[1] = write end */

Data written to `fds[1]` is buffered by the kernel until it is
read from `fds[0]`. Reading drains the buffer; writing blocks
when the buffer is full. The pipe's capacity is a few tens of
kilobytes, so a pipe cannot be used to store a large file.

The most important property: **pipes have no names**. They are
not files in the filesystem and cannot be opened by path.
They exist only as long as the file descriptors referring to
them exist. When every write end is closed, readers see EOF.

## What the Program Shows

| Demo | Idea | Key Calls |
|------|------|-----------|
| 1. Same process | Write, close, read | `pipe`, `write`, `read` |
| 2. fork() | Parent writes, child reads | `pipe`, `fork`, `close`, `waitpid` |
| 3. exec() | Pipe into `wc -l` | `pipe`, `fork`, `dup2`, `execlp` |

## Usage

    ./pipe_demo

Runs all three demos in sequence and prints the output.

## Build

    gcc -Wall -Wextra -std=c99 -o pipe_demo pipe_demo.c

## Skills Practiced

- **`pipe()`** — creates two file descriptors in `fds[0]` and `fds[1]`
- **`fork()`** — shares the pipe across the parent/child boundary
- **Closing unused ends** — mandatory for EOF to work
- **`read()` returning 0** — the only signal that all write ends are closed
- **`dup2()`** — wire a pipe into stdin (or stdout) of an exec'd process
- **`execlp()`** — replace the current process image with another program
- **`waitpid()`** — reap children and check exit status

## Closing Both Ends

The most common beginner mistake is forgetting to close the unused
end of the pipe. The rules:

| Process | Closes |
|---------|--------|
| Reader | its write end (otherwise it will never see EOF) |
| Writer | its read end (harmless but tidy) |

If the reader keeps the write end open, `read()` blocks forever
waiting for data that will never come — the reader itself is one
of the potential writers. Every tutorial on pipes says this, and
every beginner hits it anyway.

## How the Shell Uses Pipes

The shell command

    ls | grep README

is implemented, roughly, as:

    pipe(fds);
    if (fork() == 0) {
        dup2(fds[1], STDOUT_FILENO);   /* ls's stdout -> pipe */
        close(fds[0]);
        close(fds[1]);
        execlp("ls", "ls", NULL);
    }
    if (fork() == 0) {
        dup2(fds[0], STDIN_FILENO);    /* pipe -> grep's stdin */
        close(fds[0]);
        close(fds[1]);
        execlp("grep", "grep", "README", NULL);
    }
    close(fds[0]);
    close(fds[1]);
    wait(NULL); wait(NULL);

That's `pipe_demo`'s Demo 3 with `ls` on the left and `grep` on
the right instead of `wc -l` and a hardcoded buffer.

## Why Pipe Is "Anonymous"

Named pipes (`mkfifo`) are the other kind: they live in the
filesystem and any process can open them by path. Anonymous
pipes have no path; they exist only while at least one
process holds a file descriptor referring to them.

| | Anonymous pipe | Named pipe (FIFO) |
|---|----------------|-------------------|
| Has a path | no | yes |
| Created by | `pipe()` | `mkfifo()` or `mkfifo(1)` |
| Process relationship | must share ancestor | any |
| Lifetime | until all fds closed | until unlinked |

For 99% of shell and inter-process work, anonymous pipes are
the right tool. Named pipes are for persistent connections
between unrelated programs.

## A Subtle Detail: Atomicity

Writes smaller than `PIPE_BUF` (4096 on Linux) are **atomic** —
the kernel writes all of them or none. Larger writes may be
split. If two processes are writing to the same pipe (unusual
but not impossible), small writes interleave safely; large
writes may not. This is why log lines are usually short.

## Notes

- **Reading drains the pipe.** Once you've read the data, it's
  gone. Pipes are not seeking channels.
- **Writes block when the pipe is full.** Reading in the same
  process before writing is what makes Demo 1 safe.
- **A pipe with no readers blocks writes.** If you write to a
  pipe whose read end is closed, the writer gets `SIGPIPE`
  (default: terminate).
- **`wc -l` was chosen** for Demo 3 because it's tiny,
  universally available, and its output is one number —
  easy to verify visually.
- **`execlp` searches `$PATH`.** Use `execv` with an absolute
  path if you want to be explicit.
- **The pipe buffer is finite.** Writing megabytes into a pipe
  without reading will block the writer. That's not a bug;
  it's flow control.

## What You Can Extend

- **A mini shell** that parses `cmd1 | cmd2 | cmd3` and wires
  the pipes for each stage
- **Bidirectional communication** with two pipes: one in each
  direction
- **A producer/consumer demo** with `fork()` and a larger
  stream, showing that the pipe's buffer actually does fill
- **Named pipes with `mkfifo`** — compare and contrast
- **Socket-based pipes with `socketpair`** — bidirectional
  instead of one-way
- **SIGPIPE handling** — ignore SIGPIPE, return `EPIPE` from
  `write`, and handle it gracefully

## Why This Matters for Linux Kernel Work

- **Pipes are implemented via the VFS layer.** A pipe is
  represented to user space as a file, but its "storage" is
  an in-kernel ring buffer of pages.
- **Pipe file descriptors** are the same abstract type as file
  descriptors for regular files, sockets, and character devices
  — one of the fundamental unifications in Unix.
- **The shell is a pipeline language.** Every `|` in a Makefile,
  every `cmd1 | cmd2` in a script, every `tee` and `xargs`
  example in a man page, is built on `pipe()` + `dup2()`.
- **Understanding pipes** is prerequisite to understanding
  Docker's `docker run | docker exec` patterns, Kubernetes
  logging sidecars, and every Unix tool that reads stdin.

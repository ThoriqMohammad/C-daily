# Day 12 — mini_cat

A minimal clone of the Unix `cat` command, built with **low-level system calls**.

## How It Works

Unlike `fopen`/`fread` (which are library wrappers), this program uses
the **Linux system calls** directly:

| System Call | Purpose |
|-------------|---------|
| `open` | Open a file, returns a file descriptor |
| `read` | Read bytes from a file descriptor |
| `write` | Write bytes to a file descriptor |
| `close` | Close a file descriptor |

## Usage

    ./mini_cat file1 [file2 ...]
    ./mini_cat < file
    ./mini_cat file1 file2 > combined

## Examples

**Print a file:**

    $ ./mini_cat hello.txt
    Hello, world!

**Concatenate files:**

    $ ./mini_cat part1.txt part2.txt > combined.txt

**Read from stdin:**

    $ echo "piped text" | ./mini_cat
    piped text

**Handle errors:**

    $ ./mini_cat missing.txt
    mini_cat: missing.txt: No such file or directory

## Build

    gcc -Wall -Wextra -o mini_cat mini_cat.c

## Skills Practiced

- **System calls**: `open`, `read`, `write`, `close`
- **File descriptors**: integers that refer to open files
- **Binary I/O**: reading raw bytes
- **Error handling**: `errno`, `strerror`
- **`argc`/`argv`**: multiple file arguments
- **Robust write loop**: handling partial writes

## Why System Calls?

| stdio (`fopen`, `fread`) | System calls (`open`, `read`) |
|--------------------------|-------------------------------|
| Buffered | Unbuffered |
| Portable | POSIX/Linux-specific |
| Higher-level | Lower-level |
| Easier | More control |

System calls are what **actually talk to the kernel**.
stdio is a wrapper around them.

## Notes

- `STDIN_FILENO` is 0, `STDOUT_FILENO` is 1, `STDERR_FILENO` is 2.
- The `read` loop handles **partial reads** and **partial writes**.
- Files are read in binary mode (no text translation).
- This is how `cat` is actually implemented—just with more options.

# Day 41 — mmap_demo

A tour of **memory mapping** with `mmap()`, `msync()`, and
`munmap()`—the syscall that attaches a file directly into a
process's address space.

## What Is mmap()?

`mmap()` asks the kernel to place a region of memory in your
address space that is **backed by a file** (or by anonymous
pages). Reading from and writing to that region becomes ordinary
memory access:

    int fd = open("file.txt", O_RDWR);
    char *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_SHARED, fd, 0);

    p[0] = 'X';        /* writes go to the page cache */
    printf("%c", p[1]);

There is no `read()` or `write()` per byte. The kernel uses the
same **page cache** that ordinary file I/O uses, so the mapping
and the file stay consistent.

## Why This Matters

| Use case | How mmap helps |
|----------|----------------|
| Dynamic loader | `ld.so` maps shared libraries with mmap |
| Databases | Index files are mapped and read with zero copy |
| Shared memory | Two processes map the same file, share pages |
| Large file editing | Edit a 1 GB file without loading it |
| Copy-on-write | `fork` shares pages until one side writes |

`malloc()` for large allocations is often implemented **in terms
of mmap**, so the syscall you are about to learn is not exotic —
it is the substrate under most of the memory in a running process.

## What the Program Shows

| Demo | Idea | Key Calls |
|------|------|-----------|
| 1. Write mapping | Create a file, write through a pointer | `open`, `ftruncate`, `mmap`, `msync` |
| 2. Read mapping | Read a file without `read()` | `open`, `fstat`, `mmap` |
| 3. Anonymous | Scratch memory with no file | `mmap(MAP_ANONYMOUS)` |
| 4. Shared view | Two mappings of the same file | Two `mmap(MAP_SHARED)` |

## Usage

    ./mmap_demo

Creates `mmap_demo_out.txt` and walks through four demos.

## Build

    gcc -Wall -Wextra -std=c99 -o mmap_demo mmap_demo.c

## Skills Practiced

- **`open` / `close`** for the underlying file
- **`ftruncate`** to give the file a size before mapping
- **`mmap`** with `PROT_READ`, `PROT_WRITE`, `MAP_SHARED`, `MAP_PRIVATE`, `MAP_ANONYMOUS`
- **`msync`** to flush dirty pages to disk
- **`munmap`** to detach the region
- **`fstat`** to get the file size for a read-only mapping

## The Three Big Flags

`mmap` takes two flag parameters—a **protection** (`PROT_*`) and
a **sharing** (`MAP_*`). The combinations you will actually use:

| Protection | Meaning |
|------------|---------|
| `PROT_READ` | can read |
| `PROT_WRITE` | can write |
| `PROT_EXEC` | can execute code from it |
| `PROT_NONE` | no access (used as a guard) |

| Sharing | Meaning |
|---------|---------|
| `MAP_SHARED` | writes are visible to other mappings of the same file |
| `MAP_PRIVATE` | writes are private; pages are copied on first write |
| `MAP_ANONYMOUS` | not backed by a file (scratch memory) |

The demo uses `MAP_SHARED` when it wants two mappings to see
each other, and `MAP_PRIVATE | MAP_ANONYMOUS` for scratch memory.

## Why ftruncate() Before a Write Mapping

If you map a 100-byte file and try to write at offset 1000, the
kernel will kill you with `SIGBUS`. The mapping is only valid up
to the file's current size. The demo calls `ftruncate(fd, DEMO_SIZE)`
first so the file actually has 4096 bytes before mapping.

| Step | Effect |
|------|--------|
| `open(O_CREAT)` | creates an empty file |
| `ftruncate(fd, 4096)` | file now has 4096 zero bytes on disk |
| `mmap(... 4096, MAP_SHARED, fd, 0)` | region can be read or written without SIGBUS |

This is the single most common source of "mmap worked but my
program crashed" bugs.

## Why msync() Before munmap()

Writes to a `MAP_SHARED` mapping are visible to other processes
immediately, but the **pages may not be flushed to disk** yet —
they sit in the page cache as dirty pages, and the kernel writes
them back on its own schedule.

`msync(addr, len, MS_SYNC)` forces a flush. Without it, a crash
right after `munmap` could lose the most recent writes. For a
demo, `msync` makes the behaviour deterministic; in a production
tool, whether you need it depends on your durability requirements.

## Why Two Mappings See Each Other

Demo 4 maps the same file twice. Both mappings point at the same
**physical pages** in the page cache. Writing through one is
visible through the other because they are not copies — they are
two virtual addresses for the same memory.

    virtual A -----> physical page 0  <----- virtual B
                     "Written via mmap"

`MAP_SHARED` is required for this. `MAP_PRIVATE` would give each
mapping its own copy (copy-on-write), and changes in one would
not appear in the other.

## Notes

- **The file is left on disk** (`mmap_demo_out.txt`) so you can
  inspect it. The demo overwrites it on the next run.
- **`MAP_ANONYMOUS` ignores the `fd` argument** — the demo passes
  `-1` for clarity.
- **`msync` with `MS_SYNC`** blocks until pages are on disk.
  `MS_ASYNC` just schedules a writeback and returns.
- **`SIGBUS` on out-of-file writes** is not a bug — it is the
  kernel telling you the mapping was smaller than the access.
- **`madvise`** can tell the kernel how you intend to use the
  region (`MADV_SEQUENTIAL`, `MADV_WILLNEED`, `MADV_DONTNEED`);
  worth a look once this demo feels comfortable.
- **The demo allocates 4096 bytes** — one page on most systems.
  Larger files work identically; the only concern is total
  address space, which on 64-bit Linux is enormous.

## What You Can Extend

- **A simple file copier** using mmap on both sides — usually
  faster than `read`/`write` for large files
- **A memory-mapped log writer** that appends to a log file
  without opening and closing it per message
- **A shared-memory producer/consumer** — two processes map the
  same file and synchronize with a pipe or a signal
- **A tiny "mmap-based text viewer"** that prints a fixed
  number of lines at a time
- **Compare performance**: time a 100 MB copy using `read`+`write`
  vs `mmap`+`memcpy`
- **`mprotect`** to toggle a region between read-only and
  writable as a debugging aid

## Why This Matters for Linux Kernel Work

- **`mmap` is the interface between the VFS and the memory
  manager.** It glues file inodes to page-cache pages to a
  process's virtual address space.
- **Page faults are the mechanism**: the first access to a
  mapped page traps into the kernel, which loads the page and
  updates the page table. Every "mmap is fast" claim rests on
  this being cheaper than a `read()` copy.
- **Shared libraries, `fork`, `exec`, and copy-on-write** all
  live in the same machinery. Once you understand `mmap`,
  `/proc/PID/maps` becomes readable.
- **Databases and high-performance servers** lean on mmap
  heavily — knowing when it helps and when `read`/`write` is
  faster is a real systems skill.

# Day 34 — mini_ls

Lists the contents of a directory using the **opendir() / readdir()**
family of calls—the same interface that powers `ls`, `find`, and
every file browser on Unix.

## How It Works

A directory in Unix is just a special kind of file: a list of
(name, inode) pairs. The kernel exposes it to user space through
an opaque handle called a **directory stream**:

    DIR *dir = opendir(path);           /* open the stream      */
    struct dirent *e = readdir(dir);    /* read one entry at a time */
    closedir(dir);                      /* release the stream   */

Each call to `readdir()` returns a pointer to a `struct dirent`
holding the next entry's name (`d_name`) and inode number
(`d_ino`). When the directory is exhausted, `readdir()` returns
`NULL`.

    struct dirent {
        ino_t  d_ino;       /* inode number */
        char   d_name[256]; /* entry name   */
        ...
    };

`readdir()` gives us only the **name**—not the metadata. To
get permissions and size we follow up with `lstat()` on the
full path of each entry.

## Usage

    ./mini_ls [directory]

If no argument is given, the current directory (`.`) is listed.

## Examples

    $ ./mini_ls
    drwxr-xr-x     4096  day-33-file-stats
    drwxr-xr-x     4096  day-34-mini-ls
    -rw-r--r--      124  .gitignore
    -rw-r--r--     1135  LICENSE
    -rw-r--r--     2987  README.md

    $ ./mini_ls /etc
    -rw-r--r--      533  hostname
    -rw-r--r--     2048  hosts
    drwxr-xr-x     4096  ssh
    ...

    $ ./mini_ls no-such-dir
    opendir: No such file or directory

## Build

    gcc -o mini_ls mini_ls.c

## Skills Practiced

- Directory streams: `opendir()`, `readdir()`, `closedir()`
- `struct dirent` and the `d_name` / `d_ino` fields
- Calling `lstat()` on each entry to fetch metadata
- Building full paths safely with `snprintf()`
- Skipping `.` and `..` by name
- Error handling that keeps going (per-entry `perror`, not abort)
- Reusing `st_mode` bitmasking from Day 33

## Kernel Behind the Tool

On disk, a Unix directory is a sequence of records mapping a
filename to an inode number. The VFS layer (Virtual File System)
abstracts that on top of every filesystem—ext4, XFS, tmpfs,
NFS—so the same `readdir()` call works everywhere.

Two facts worth internalizing:

- The **name lives in the directory**, not in the inode. That's
  why hard links are possible: multiple directory entries can
  point to the same inode.
- `readdir()` returns entries in **filesystem order**, which is
  not sorted. `ls` sorts in user space—`mini_ls` doesn't, to
  keep the code small.

## Notes

- We use `lstat()` (not `stat()`) so symbolic links are reported
  as links, not as their targets.
- `d_name` is guaranteed to be null-terminated but its maximum
  length is filesystem-dependent; we use `PATH_MAX_LEN` as a
  safe upper bound for the concatenated path.
- There is no sorting, no recursion, and no color output—`ls`
  has all of that, and each feature adds real complexity.
- Output columns are aligned with a fixed width for
  readability; a production tool would compute the width
  from the actual data.

## Why This Matters for Linux Kernel Work

- `readdir` sits on the **VFS boundary** between user space
  and filesystem drivers.
- Understanding the directory-as-file abstraction is
  prerequisite to reading kernel code for `ext4`, `overlayfs`,
  and similar.
- It teaches the **inode / dentry / name** separation that
  defines how Linux thinks about filesystems.
- Every recursive tool you'll ever write—`find`, `du`,
  `tar`, `rsync`—starts from exactly this loop.

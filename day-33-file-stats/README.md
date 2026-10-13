# Day 33 — file_stats

Prints metadata about a file using the **stat() syscall**—the same
kernel interface that powers `ls -l`, `stat`, and `find`.

## How It Works

Every file on a Unix filesystem has an **inode**—a kernel structure
that stores everything about the file *except* its name and contents.
The `stat()` syscall fills in a `struct stat` with that metadata:

    struct stat {
        dev_t     st_dev;      /* device */
        ino_t     st_ino;      /* inode number */
        mode_t    st_mode;     /* type + permissions */
        nlink_t   st_nlink;    /* hard link count */
        uid_t     st_uid;      /* owner */
        gid_t     st_gid;      /* group */
        off_t     st_size;     /* size in bytes */
        time_t    st_mtime;    /* last modification */
        ...
    };

The `st_mode` field packs two things into one integer:

- the **file type** (regular, directory, symlink, ...)
- the **permission bits** (rwx for user, group, other)

We extract each part with **bitmasks** and the `S_IS*` macros.

## Usage

    ./file_stats <path>

## Examples

    $ ./file_stats README.md
    File:        README.md
    Type:        regular file
    Size:        1842 bytes
    Permissions: -rw-r--r-- (0644)
    Inode:       1234567
    Links:       1
    Owner:       uid=1000
    Group:       gid=1000
    Modified:    2026-10-12 09:14:22

    $ ./file_stats /tmp
    File:        /tmp
    Type:        directory
    Size:        4096 bytes
    Permissions: drwxrwxrwt (1777)
    Inode:       42
    Links:       8
    Owner:       uid=0
    Group:       gid=0
    Modified:    2026-10-12 08:01:05

    $ ./file_stats does-not-exist
    stat: No such file or directory

## Build

    gcc -o file_stats file_stats.c

## Skills Practiced

- The `stat()` syscall and `struct stat`
- **Bitmasking** with `st_mode`
- The `S_ISREG`, `S_ISDIR`, `S_ISLNK`, ... type macros
- The `S_IRUSR`, `S_IWUSR`, `S_IXUSR`, ... permission macros
- Octal output with `printf("%o", ...)`
- Time formatting with `localtime()` and `strftime()`
- Error handling with `perror()`

## Kernel Behind the Tool

In Unix, **"everything is a file"**—and the kernel keeps a uniform
metadata record for every one of them. The `stat` family of syscalls
(`stat`, `lstat`, `fstat`) exposes that record to user space:

| Syscall | Follows symlinks? | Takes |
|---------|-------------------|-------|
| `stat`  | yes               | path  |
| `lstat` | no                | path  |
| `fstat` | n/a               | fd    |

The `st_mode` field is 16 bits wide. The top 4 bits encode the file
type, the bottom 12 encode permissions (including setuid, setgid,
and sticky bits). We only print the low 12 with `& 07777`.

## Notes

- The output is **informational**, not a replacement for `stat(1)`.
- Times are shown in the **local timezone**; the raw `st_mtime`
  is seconds since the Unix epoch.
- `st_ino` is only meaningful within a single filesystem.
- On some filesystems (e.g. FAT), inodes and link counts are
  synthesized—values may look odd.

## Why This Matters for Linux Kernel Work

- `stat` is one of the **oldest and most-used syscalls** in Unix.
- Understanding `st_mode` bitmasking is the same skill used
  for `chmod`, `umask`, and access-control checks.
- It introduces the **inode abstraction**—central to how the
  kernel's VFS layer organizes storage.
- Every tool that lists, copies, or syncs files calls `stat`
  under the hood.

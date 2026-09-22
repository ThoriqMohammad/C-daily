/* mini_cat.c
 *
 * A minimal clone of the Unix `cat` command.
 * Uses low-level system calls: open, read, write, close.
 *
 * Usage:
 *   ./mini_cat file1 [file2 ...]
 *   ./mini_cat < file          (reads from stdin)
 *   ./mini_cat file1 file2     (concatenates multiple files)
 *
 * Skills practiced:
 *   - System calls (open, read, write, close)
 *   - File descriptors
 *   - Error handling with errno
 *   - Buffer management
 *   - argc/argv
 *   - Linux programming interface
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define BUF_SIZE 4096

/* Copy all data from fd to stdout */
void cat_fd(int fd)
{
    char buf[BUF_SIZE];
    ssize_t n;

    while ((n = read(fd, buf, BUF_SIZE)) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(STDOUT_FILENO, buf + written, n - written);
            if (w < 0) {
                fprintf(stderr, "mini_cat: write error: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            written += w;
        }
    }

    if (n < 0) {
        fprintf(stderr, "mini_cat: read error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    /* No files: read from stdin */
    if (argc == 1) {
        cat_fd(STDIN_FILENO);
        return 0;
    }

    /* One or more files: read each in order */
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "mini_cat: %s: %s\n", argv[i], strerror(errno));
            continue;  /* skip this file, try the next */
        }

        cat_fd(fd);
        close(fd);
    }

    return 0;
}

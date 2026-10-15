/* dup2_demo.c
 *
 * Demonstrates file descriptor duplication with dup() and dup2().
 *
 * Every process starts with three open file descriptors:
 *
 *   0  stdin
 *   1  stdout
 *   2  stderr
 *
 * dup() and dup2() let us copy one descriptor to another number,
 * which is how shells implement redirection:
 *
 *   ./program > out.txt     # stdout (1) points at out.txt
 *   ./program 2>&1          # stderr (2) points where stdout (1) points
 *
 * This tool walks through four small experiments:
 *
 *   1. dup() and print both descriptors      (two fds, same file)
 *   2. dup2() to redirect stdout to a file   (printf goes to disk)
 *   3. restore original stdout with dup2()   (save and swap back)
 *   4. 2>&1-style merge of stderr into stdout
 *
 * Usage:
 *   ./dup2_demo
 *
 * Skills practiced:
 *   - File descriptors as small integers
 *   - dup() and dup2() semantics
 *   - fflush() before swapping stdout
 *   - open() with O_WRONLY | O_CREAT | O_TRUNC
 *   - Saving and restoring fd 1
 *   - Error handling with perror()
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define OUT_FILE "dup2_demo_out.txt"

/* Demo 1: dup() creates a second handle to the same open file */
static void demo_dup(void)
{
    int copy;

    printf("--- 1. dup() ---\n");

    copy = dup(STDOUT_FILENO);
    if (copy == -1)
    {
        perror("dup");
        exit(EXIT_FAILURE);
    }

    printf("STDOUT_FILENO = %d\n", STDOUT_FILENO);
    printf("dup returned  = %d\n", copy);

    /* Both descriptors write to the same place */
    write(STDOUT_FILENO, "via original fd\n", 16);
    write(copy,            "via dup copy   \n", 16);

    close(copy);
    printf("\n");
}

/* Demo 2: dup2() redirects stdout to a file */
static void demo_redirect(void)
{
    int fd;

    printf("--- 2. dup2() redirect stdout ---\n");
    printf("This line still goes to the terminal.\n");

    fd = open(OUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    /* Flush before swapping: printf buffers, and we don't want
     * earlier text landing in the file after the redirect. */
    fflush(stdout);

    if (dup2(fd, STDOUT_FILENO) == -1)
    {
        perror("dup2");
        exit(EXIT_FAILURE);
    }
    close(fd);

    printf("This line goes to %s instead.\n", OUT_FILE);
    fflush(stdout);
}

/* Demo 3: save fd 1, redirect, then restore */
static void demo_restore(void)
{
    int saved, fd;

    printf("--- 3. Save and restore stdout ---\n");

    saved = dup(STDOUT_FILENO);
    if (saved == -1)
    {
        perror("dup");
        exit(EXIT_FAILURE);
    }

    fd = open(OUT_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    fflush(stdout);
    if (dup2(fd, STDOUT_FILENO) == -1)
    {
        perror("dup2");
        exit(EXIT_FAILURE);
    }
    close(fd);

    printf("(appended to %s)\n", OUT_FILE);
    fflush(stdout);

    /* Restore the original stdout */
    if (dup2(saved, STDOUT_FILENO) == -1)
    {
        perror("dup2");
        exit(EXIT_FAILURE);
    }
    close(saved);

    printf("Back on the terminal.\n\n");
}

/* Demo 4: merge stderr into stdout (the shell's 2>&1) */
static void demo_merge_stderr(void)
{
    printf("--- 4. Merge stderr into stdout (2>&1) ---\n");

    if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1)
    {
        perror("dup2");
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "this came from stdout\n");
    fprintf(stderr, "this came from stderr\n");
    printf("(both visible in the same stream)\n\n");
}

int main(void)
{
    printf("====================================\n");
    printf("            DUP2 DEMO               \n");
    printf("====================================\n\n");

    demo_dup();
    demo_redirect();
    demo_restore();
    demo_merge_stderr();

    printf("Done. %s was written during the demo.\n", OUT_FILE);
    return 0;
}

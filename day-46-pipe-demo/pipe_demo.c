/* pipe_demo.c
 *
 * Demonstrates anonymous pipes with pipe(), fork(), and read()/write().
 *
 * An anonymous pipe is a one-way byte channel between two file
 * descriptors in the same process, or between a parent and child
 * after fork(). It is the mechanism behind the shell's `|`
 * operator:
 *
 *   ls | grep README
 *
 * The shell creates a pipe, forks twice, and uses dup2() to
 * connect the first child's stdout to the pipe's write end and
 * the second child's stdin to the pipe's read end.
 *
 * This tool walks through three experiments:
 *
 *   1. pipe() alone: write into a pipe, read it back
 *   2. fork(): parent writes, child reads
 *   3. exec(): run `wc -l` with stdin from a pipe (mini shell)
 *
 * Usage:
 *   ./pipe_demo
 *
 * Skills practiced:
 *   - pipe() returns two fds: [0] = read end, [1] = write end
 *   - fork() shares the pipe across the parent/child boundary
 *   - close() discipline: closing unused ends so EOF works
 *   - read() returning 0 on EOF (all write ends closed)
 *   - dup2() to wire a pipe to stdin of an exec'd process
 *   - waitpid() to reap children
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* ---------- Demo 1: pipe in a single process ---------- */

static void demo_same_process(void)
{
    int fds[2];

    printf("--- 1. pipe() in a single process ---\n");

    if (pipe(fds) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    const char *msg = "hello through a pipe\n";
    ssize_t written = write(fds[1], msg, strlen(msg));
    printf("Wrote %zd bytes into fd %d\n", written, fds[1]);

    /* Close the write end so read() sees EOF when the pipe is drained */
    close(fds[1]);

    char buf[256];
    ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
    if (n == -1)
    {
        perror("read");
        close(fds[0]);
        exit(EXIT_FAILURE);
    }
    buf[n] = '\0';

    printf("Read %zd bytes from fd %d: %s", n, fds[0], buf);
    close(fds[0]);
    printf("\n");
}

/* ---------- Demo 2: parent writes, child reads ---------- */

static void demo_fork(void)
{
    int fds[2];
    pid_t pid;

    printf("--- 2. fork(): parent writes, child reads ---\n");

    if (pipe(fds) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    {
        /* Child: close write end (it only reads), then read until EOF */
        close(fds[1]);

        char buf[256];
        ssize_t n;
        printf("[child]  reading from pipe...\n");
        while ((n = read(fds[0], buf, sizeof(buf) - 1)) > 0)
        {
            buf[n] = '\0';
            printf("[child]  got: %s", buf);
        }
        if (n == -1)
            perror("read");

        close(fds[0]);
        printf("[child]  EOF, exiting\n");
        exit(EXIT_SUCCESS);
    }

    /* Parent: close read end (it only writes) */
    close(fds[0]);
    const char *msg = "line one\nline two\nline three\n";
    printf("[parent] writing %zu bytes\n", strlen(msg));
    if (write(fds[1], msg, strlen(msg)) == -1)
        perror("write");
    close(fds[1]);   /* signals EOF to the child */

    int status;
    waitpid(pid, &status, 0);
    printf("[parent] child exited with status %d\n\n",
           WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

/* ---------- Demo 3: pipe into a child running `wc -l` ---------- */

static void demo_exec_wc(void)
{
    int fds[2];
    pid_t pid;

    printf("--- 3. pipe() into `wc -l` (mini shell) ---\n");

    if (pipe(fds) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    {
        /* Child: wire pipe read end to stdin, then exec wc -l */
        if (dup2(fds[0], STDIN_FILENO) == -1)
        {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(fds[0]);
        close(fds[1]);

        execlp("wc", "wc", "-l", (char *)NULL);

        /* If execlp returns, it failed */
        perror("execlp");
        exit(EXIT_FAILURE);
    }

    /* Parent: write three lines into the pipe */
    close(fds[0]);
    const char *lines =
        "first line\n"
        "second line\n"
        "third line\n";
    if (write(fds[1], lines, strlen(lines)) == -1)
        perror("write");
    close(fds[1]);   /* EOF for wc */

    int status;
    waitpid(pid, &status, 0);
    printf("[parent] wc exited with status %d\n\n",
           WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

int main(void)
{
    printf("====================================\n");
    printf("           PIPE DEMO                \n");
    printf("====================================\n\n");

    demo_same_process();
    demo_fork();
    demo_exec_wc();

    printf("Done.\n");
    return 0;
}

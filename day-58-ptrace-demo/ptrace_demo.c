/* ptrace_demo.c
 *
 * Trace a child process's syscalls with ptrace, like a tiny strace.
 *
 * ptrace is the system call that lets one process ("the tracer")
 * observe and control another ("the tracee"). It is the
 * foundation of every debugger, every syscall tracer, and every
 * dynamic analysis tool on Linux.
 *
 * What it enables:
 *   - gdb: breakpoints, single-stepping, memory inspection
 *   - strace: syscall tracing
 *   - ltrace: library call tracing
 *   - perf: profiling
 *   - seccomp: sandboxing (via ptrace-based enforcement)
 *
 * The protocol:
 *
 *   1. Parent forks a child.
 *   2. Child calls ptrace(PTRACE_TRACEME) and then exec().
 *   3. Parent calls ptrace(PTRACE_SETOPTIONS) to enable
 *      syscall-stops.
 *   4. Parent calls waitpid() to wait for the child to stop.
 *   5. Each time the child makes a syscall, it stops twice:
 *        once on entry, once on exit.
 *   6. Parent uses ptrace(PTRACE_GETREGS) to read the child's
 *      registers, which tell us which syscall it is and what
 *      its arguments are.
 *   7. Parent calls ptrace(PTRACE_SYSCALL) to let the child
 *      continue to the next stop.
 *
 * This demo traces a small program (a child that opens a file
 * and writes to it) and prints each syscall with its return
 * value. That's essentially strace, minus the decoder.
 *
 * Usage:
 *   ./ptrace_demo
 *
 * Skills practiced:
 *   - ptrace() and its request types
 *   - PTRACE_TRACEME, PTRACE_SETOPTIONS, PTRACE_SYSCALL
 *   - PTRACE_GETREGS and struct user_regs_struct
 *   - PTRACE_O_TRACESYSGOOD for distinguishing syscall-stops
 *   - waitpid() with WIFSTOPPED
 *   - Reading the child's registers to identify syscalls
 *   - The ptrace "parent-child" debugging model
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/syscall.h>

/* Map a small subset of syscall numbers to names.
 * A real strace decodes hundreds; we cover the ones our
 * demo child actually makes. */
static const char *syscall_name(long nr)
{
    switch (nr)
    {
        case SYS_write:   return "write";
        case SYS_read:    return "read";
        case SYS_openat:  return "openat";
        case SYS_open:    return "open";
        case SYS_close:   return "close";
        case SYS_exit:    return "exit";
        case SYS_exit_group: return "exit_group";
        case SYS_mmap:    return "mmap";
        case SYS_munmap:  return "munmap";
        case SYS_brk:     return "brk";
        case SYS_fstat:   return "fstat";
        case SYS_newfstatat: return "newfstatat";
        case SYS_arch_prctl: return "arch_prctl";
        case SYS_set_tid_address: return "set_tid_address";
        case SYS_set_robust_list: return "set_robust_list";
        case SYS_prlimit64: return "prlimit64";
        case SYS_rseq:    return "rseq";
        case SYS_getrandom: return "getrandom";
        default:          return "?";
    }
}

/* The child: does some simple I/O so we have syscalls to trace */
static void child_main(void)
{
    /* Write to stdout */
    const char *msg = "hello from the traced child\n";
    write(STDOUT_FILENO, msg, strlen(msg));

    /* Open and write a temp file */
    int fd = open("/tmp/ptrace_demo_file.txt",
                  O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0)
    {
        const char *line = "data written by the child\n";
        write(fd, line, strlen(line));
        close(fd);
    }

    /* Write to stderr too */
    const char *err = "and one line to stderr\n";
    write(STDERR_FILENO, err, strlen(err));

    /* exit() calls exit_group on Linux */
    exit(0);
}

int main(void)
{
    printf("====================================\n");
    printf("           PTRACE DEMO              \n");
    printf("====================================\n\n");

    pid_t child = fork();
    if (child == -1)
    {
        perror("fork");
        return 1;
    }

    if (child == 0)
    {
        /* Child: request tracing by parent, then exec.
         * We don't actually exec here — we stay in the same
         * binary and run child_main() directly. That's fine
         * for ptrace: PTRACE_TRACEME followed by any code
         * works.
         *
         * To trace a *different* program, this would be:
         *   ptrace(PTRACE_TRACEME, 0, 0, 0);
         *   execl("/bin/ls", "ls", NULL);
         */
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1)
        {
            perror("ptrace(PTRACE_TRACEME)");
            _exit(1);
        }

        /* Stop ourselves. The parent will see this via waitpid
         * and know we've become traceable. */
        raise(SIGSTOP);

        child_main();
        _exit(0);   /* never reached */
    }

    /* Parent: wait for the child's initial SIGSTOP */
    int status;
    if (waitpid(child, &status, 0) == -1)
    {
        perror("waitpid");
        return 1;
    }

    if (!WIFSTOPPED(status))
    {
        fprintf(stderr, "Child did not stop as expected.\n");
        return 1;
    }

    /* Enable syscall-stops and make waitpid report syscall-stops
     * as SIGTRAP|0x80 (so we can distinguish them from other
     * signals). PTRACE_O_TRACESYSGOOD is the standard way. */
    if (ptrace(PTRACE_SETOPTIONS, child, 0,
               (void *)(PTRACE_O_TRACESYSGOOD)) == -1)
    {
        perror("ptrace(PTRACE_SETOPTIONS)");
        return 1;
    }

    printf("[parent] tracing child pid %d\n\n", (int)child);

    /* The syscall-stop loop.
     *
     * At each iteration, the child is stopped either:
     *   - just before a syscall (entry), or
     *   - just after a syscall (exit).
     *
     * We track which phase we're in with a flag. On entry, we
     * read rax (syscall number) and the first three args from
     * rdi, rsi, rdx. On exit, we read rax again (return value).
     *
     * NOTE: this is x86-64 specific. On ARM, the syscall number
     * is in x8, args in x0-x5, return in x0. Cross-architecture
     * tracing requires per-arch register handling (this is why
     * strace has dozens of arch-specific source files). */

    int in_syscall = 0;
    long syscall_entry = -1;

    for (;;)
    {
        /* Resume the child until the next syscall-stop or signal.
         * PTRACE_SYSCALL is like PTRACE_CONT but stops on
         * syscall boundaries. */
        if (ptrace(PTRACE_SYSCALL, child, 0, 0) == -1)
        {
            perror("ptrace(PTRACE_SYSCALL)");
            return 1;
        }

        if (waitpid(child, &status, 0) == -1)
        {
            perror("waitpid");
            return 1;
        }

        if (WIFEXITED(status))
        {
            printf("\n[parent] child exited with code %d\n",
                   WEXITSTATUS(status));
            break;
        }

        if (WIFSIGNALED(status))
        {
            printf("\n[parent] child killed by signal %d\n",
                   WTERMSIG(status));
            break;
        }

        if (!WIFSTOPPED(status))
            continue;

        int sig = WSTOPSIG(status);

        /* Distinguish syscall-stops from other signals */
        if (sig == (SIGTRAP | 0x80))
        {
            /* This is a syscall-stop */
            struct user_regs_struct regs;
            if (ptrace(PTRACE_GETREGS, child, 0, &regs) == -1)
            {
                perror("ptrace(PTRACE_GETREGS)");
                return 1;
            }

            if (!in_syscall)
            {
                /* Syscall entry: record the number */
                syscall_entry = (long)regs.orig_rax;
                in_syscall = 1;
            }
            else
            {
                /* Syscall exit: rax holds the return value */
                long ret = (long)regs.rax;
                const char *name = syscall_name(syscall_entry);

                if (strcmp(name, "?") != 0)
                {
                    printf("[child] %-20s = %ld\n", name, ret);
                }
                else
                {
                    printf("[child] syscall %-12ld = %ld\n",
                           syscall_entry, ret);
                }

                in_syscall = 0;
            }
        }
        else
        {
            /* Some other signal. Deliver it to the child so
             * its normal behavior resumes. SIGSTOP, SIGTSTP,
             * and SIGCONT can't be delivered this way; others
             * can. For a demo, we just pass signal 0 which
             * means "don't deliver anything". */
            if (ptrace(PTRACE_SYSCALL, child, 0, (void *)(long)sig) == -1)
            {
                perror("ptrace(PTRACE_SYSCALL signal)");
                return 1;
            }
        }
    }

    printf("\n[parent] trace complete\n");
    return 0;
}

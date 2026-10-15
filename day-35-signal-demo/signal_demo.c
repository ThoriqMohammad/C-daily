/* signal_demo.c
 *
 * A tour of POSIX signals on Linux.
 *
 * A signal is a small asynchronous notification the kernel
 * delivers to a process: "something happened, deal with it."
 * Ctrl+C, a crash, a timer, or another process can all
 * trigger one.
 *
 * Demonstrates:
 *   - Installing a handler with sigaction()
 *   - Sending a signal to yourself with raise()
 *   - Blocking signals and checking pending ones
 *   - Why standard signals are NOT queued
 *   - Killing a child process with kill()
 *   - Waiting for a timer signal safely with sigsuspend()
 *   - Signals that cannot be caught (SIGKILL)
 *
 * Usage:
 *   ./signal_demo
 *
 * Skills practiced:
 *   - Signals (sigaction, raise, kill, alarm)
 *   - Signal masks (sigprocmask, sigpending, sigsuspend)
 *   - Processes (fork, waitpid)
 *   - volatile sig_atomic_t
 *   - Error handling
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Flags shared with handlers. Only sig_atomic_t is safe to
 * write from a handler, and volatile stops the compiler from
 * caching it in a register. */
static volatile sig_atomic_t got_sigint = 0;
static volatile sig_atomic_t usr1_count = 0;
static volatile sig_atomic_t got_alarm = 0;

/* Handlers: keep them tiny. Just set a flag and return. */
static void handle_sigint(int sig)
{
    (void)sig;
    got_sigint = 1;
}

static void handle_sigusr1(int sig)
{
    (void)sig;
    usr1_count++;
}

static void handle_sigalrm(int sig)
{
    (void)sig;
    got_alarm = 1;
}

/* Install a handler using sigaction (preferred over signal()) */
static void install_handler(int signo, void (*handler)(int))
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(signo, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

/* Demo 1: catch a signal we send to ourselves */
static void demo_catch(void)
{
    printf("--- 1. Catching SIGINT ---\n");
    install_handler(SIGINT, handle_sigint);

    printf("Sending SIGINT to myself with raise()\n");
    raise(SIGINT);

    printf("Handler ran: %s\n\n", got_sigint ? "yes" : "no");
}

/* Demo 2: block a signal, send it twice, unblock it */
static void demo_block_and_pending(void)
{
    sigset_t block, old, pending;

    printf("--- 2. Blocking and pending signals ---\n");
    install_handler(SIGUSR1, handle_sigusr1);
    usr1_count = 0;

    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);
    sigprocmask(SIG_BLOCK, &block, &old);

    printf("SIGUSR1 blocked. Sending it twice.\n");
    raise(SIGUSR1);
    raise(SIGUSR1);

    sigpending(&pending);
    printf("SIGUSR1 pending: %s\n", sigismember(&pending, SIGUSR1) ? "yes" : "no");
    printf("Handler runs so far: %d\n", (int)usr1_count);

    printf("Unblocking SIGUSR1\n");
    sigprocmask(SIG_SETMASK, &old, NULL);

    printf("Handler runs after unblock: %d\n", (int)usr1_count);
    printf("Two signals sent, one delivered: standard signals do not queue.\n\n");
}

/* Demo 3: kill a child process with a signal */
static void demo_kill_child(void)
{
    int status;
    pid_t pid;

    printf("--- 3. Killing a child with kill() ---\n");

    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    {
        /* Child: sleep until a signal arrives */
        for (;;)
            pause();
    }

    printf("Parent: sending SIGTERM to child %d\n", (int)pid);
    kill(pid, SIGTERM);
    waitpid(pid, &status, 0);

    if (WIFSIGNALED(status))
        printf("Child ended by signal %d (%s)\n\n",
               WTERMSIG(status), strsignal(WTERMSIG(status)));
}

/* Demo 4: wait for a timer without a race condition */
static void demo_alarm(void)
{
    sigset_t block, old;

    printf("--- 4. Timer with alarm() and sigsuspend() ---\n");
    install_handler(SIGALRM, handle_sigalrm);
    got_alarm = 0;

    /* Block SIGALRM first, so it cannot slip in before we wait */
    sigemptyset(&block);
    sigaddset(&block, SIGALRM);
    sigprocmask(SIG_BLOCK, &block, &old);

    printf("Setting alarm for 1 second...\n");
    alarm(1);

    /* sigsuspend atomically unblocks and sleeps */
    while (!got_alarm)
        sigsuspend(&old);

    sigprocmask(SIG_SETMASK, &old, NULL);
    printf("SIGALRM received.\n\n");
}

/* Demo 5: some signals cannot be caught */
static void demo_uncatchable(void)
{
    struct sigaction sa;

    printf("--- 5. Signals that cannot be caught ---\n");

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGKILL, &sa, NULL) == -1)
        printf("sigaction(SIGKILL) failed: %s\n", strerror(errno));

    if (sigaction(SIGSTOP, &sa, NULL) == -1)
        printf("sigaction(SIGSTOP) failed: %s\n", strerror(errno));

    printf("The kernel always reserves these two.\n");
}

int main(void)
{
    printf("====================================\n");
    printf("          SIGNAL DEMO               \n");
    printf("====================================\n\n");

    demo_catch();
    demo_block_and_pending();
    demo_kill_child();
    demo_alarm();
    demo_uncatchable();

    return 0;
}

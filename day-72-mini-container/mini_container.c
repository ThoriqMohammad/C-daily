/* mini_container.c
 *
 * Step order (what this program does):
 *
 *   1. Parse command-line args (rootfs path, command to run)
 *   2. Check privileges (needs CAP_SYS_ADMIN; typically root)
 *   3. Set up isolation, in order:
 *      a. unshare(CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS |
 *                 CLONE_NEWIPC | CLONE_NEWNET)
 *         - new PID namespace (process appears as PID 1)
 *         - new UTS namespace (own hostname)
 *         - new mount namespace (mounts are private)
 *         - new IPC namespace (own SysV/POSIX IPC objects)
 *         - new network namespace (own loopback, no external net)
 *      b. fork() so the child is the first process in the new
 *         PID namespace
 *   4. In the child:
 *      a. chdir to the new rootfs
 *      b. chroot to the new rootfs (confine filesystem view)
 *      c. chdir("/") (chroot doesn't change cwd)
 *      d. mount /proc inside the container (for ps, /proc reads)
 *      e. set hostname
 *      f. exec the requested command
 *   5. In the parent: waitpid for the child, report exit status
 *
 * What this program does NOT do (and Docker/K8s do):
 *   - cgroups (resource limits: CPU, memory, I/O)
 *   - seccomp (syscall filtering)
 *   - capabilities dropping (the container has full root)
 *   - layered filesystems (overlayfs, block devices)
 *   - networking (a veth pair to give it real connectivity)
 *   - image distribution (registry, tarball, docker pull)
 *   - init process (PID 1 signal forwarding, zombie reaping)
 *   - user namespaces (running as unprivileged user)
 *   - SELinux/AppArmor profiles
 *   - logging, health checks, restart policies
 *   Each is a real feature of production containers; none are
 *   needed to understand the core isolation primitives.
 *
 * What this program DOES demonstrate:
 *   - Namespace isolation (PID, UTS, mount, IPC, net)
 *   - Filesystem confinement via chroot
 *   - The "one process, isolated view" model
 *   - Why PID 1 inside the container matters
 *   - The unshare() syscall that starts the whole thing
 *
 * Usage:
 *   sudo ./mini_container <rootfs> <command> [args...]
 *
 * Example:
 *   sudo mkdir -p /tmp/miniroot/bin
 *   sudo cp /bin/sh /tmp/miniroot/bin/sh
 *   sudo cp /bin/ls /tmp/miniroot/bin/ls
 *   # (copy needed libraries too - see README for details)
 *   sudo ./mini_container /tmp/miniroot /bin/sh
 *
 * Or for a quick test that doesn't need a full rootfs:
 *   sudo ./mini_container / /bin/sh
 *
 * Skills practiced:
 *   - unshare(), clone(), chroot() — the core isolation syscalls
 *   - Linux namespaces (PID, UTS, mount, IPC, net)
 *   - PID 1 semantics inside a namespace
 *   - Filesystem isolation with chroot
 *   - Mounting /proc inside a namespace
 *   - Why capabilities (root) are required
 *   - What Docker actually does underneath
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/syscall.h>

/* ---------- child setup (runs inside the new namespaces) ---------- */

static int container_child(const char *rootfs,
                           const char *hostname,
                           char *const argv[])
{
    /* The child has already been unshared by the parent. Here
     * we set up the filesystem view, then exec. */

    /* chroot requires the new root to exist and be a directory */
    struct stat st;
    if (stat(rootfs, &st) == -1 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "mini_container: rootfs %s is not a directory\n",
                rootfs);
        return 1;
    }

    /* Change to the new root before chroot() so the cwd is inside
     * the new filesystem. (chroot() itself doesn't change cwd;
     * without this step, the process could still access files
     * outside the new root via a relative path from the old cwd.) */
    if (chdir(rootfs) == -1)
    {
        perror("chdir(rootfs)");
        return 1;
    }

    if (chroot(rootfs) == -1)
    {
        perror("chroot");
        return 1;
    }

    /* After chroot, "/" is the new root. Do it again so relative
     * paths work naturally. */
    if (chdir("/") == -1)
    {
        perror("chdir(/)");
        return 1;
    }

    /* Mount /proc inside the new root. Without this, "ps" and
     * tools that read /proc/<pid>/ would see nothing (or the
     * wrong /proc — the parent's). The mount is private because
     * we're in a new mount namespace. */
    struct stat proc_st;
    if (stat("/proc", &proc_st) == 0)
    {
        if (mount("proc", "/proc", "proc", 0, NULL) == -1)
        {
            /* Not fatal — some rootfs don't have /proc, and
             * some sandboxes don't allow mounting. Warn and
             * continue. */
            fprintf(stderr, "mini_container: warning: mount /proc: %s\n",
                    strerror(errno));
        }
    }

    /* Set hostname (only works because we're in a new UTS namespace) */
    if (hostname && *hostname)
    {
        if (sethostname(hostname, strlen(hostname)) == -1)
        {
            fprintf(stderr, "mini_container: warning: sethostname: %s\n",
                    strerror(errno));
        }
    }

    /* Exec the user's command. If this succeeds, the child process
     * is replaced by the new program; if it fails, we print an
     * error and exit. */
    execvp(argv[0], argv);

    fprintf(stderr, "mini_container: exec %s: %s\n",
            argv[0], strerror(errno));
    return 127;
}

/* ---------- main ---------- */

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr,
                "Usage: %s <rootfs> <command> [args...]\n"
                "Example: sudo %s / /bin/sh\n"
                "         sudo %s /tmp/miniroot /bin/ls\n",
                argv[0], argv[0], argv[0]);
        return 1;
    }

    const char *rootfs = argv[1];
    char *const *cmd = &argv[2];

    /* unshare() requires CAP_SYS_ADMIN, which usually means root.
     * A real container runtime would use user namespaces to allow
     * unprivileged containers, but that adds a lot of complexity
     * (UID mapping, etc). This demo requires root. */
    if (geteuid() != 0)
    {
        fprintf(stderr,
                "mini_container: must run as root (try: sudo %s ...)\n",
                argv[0]);
        return 1;
    }

    /* Set up all the namespace flags. Each CLONE_NEW* flag creates
     * a new namespace of that kind. The process that calls unshare()
     * moves into the new namespaces; its current view of the system
     * becomes isolated. */
    int flags = CLONE_NEWPID   /* new process ID namespace */
              | CLONE_NEWUTS   /* new hostname namespace */
              | CLONE_NEWNS    /* new mount namespace */
              | CLONE_NEWIPC   /* new System V IPC namespace */
              | CLONE_NEWNET;  /* new network namespace */

    /* Note on PID namespace + fork:
     * Creating a new PID namespace only affects CHILDREN created
     * AFTER the unshare. The calling process is NOT put into the
     * new PID namespace. So we unshare, then fork, and the child
     * is PID 1 in the new namespace. */
    if (unshare(flags) == -1)
    {
        perror("unshare");
        return 1;
    }

    /* Fork. The child becomes the container init (PID 1). */
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return 1;
    }

    if (pid == 0)
    {
        /* Child: run the container */
        int rc = container_child(rootfs, "mini-container", cmd);
        _exit(rc);
    }

    /* Parent: wait for the child, then report exit status. */
    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status))
    {
        int code = WEXITSTATUS(status);
        printf("[mini_container] exited with status %d\n", code);
        return code;
    }
    if (WIFSIGNALED(status))
    {
        int sig = WTERMSIG(status);
        printf("[mini_container] killed by signal %d (%s)\n",
               sig, strsignal(sig));
        return 128 + sig;
    }

    return 0;
}

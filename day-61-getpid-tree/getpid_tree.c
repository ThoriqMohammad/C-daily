/* getpid_tree.c
 *
 * Enumerate every process on the system and print the process tree.
 *
 * On Linux, /proc is a pseudo-filesystem that exposes kernel state
 * as files and directories. Every running process has a directory
 * at /proc/<pid>/ containing files like:
 *
 *   cmdline    the command line (null-separated)
 *   comm       the process name (15 characters, no spaces)
 *   status     key: value lines (PPid, State, Uid, ...)
 *   stat       one-line summary of process state
 *
 * This program:
 *
 *   1. Scans /proc for numeric directory names (PIDs)
 *   2. For each, reads /proc/<pid>/status to get the parent PID
 *   3. Builds a tree from parent -> children
 *   4. Prints the tree rooted at PID 1 (init/systemd)
 *
 * That's how pstree works, minus the fancy rendering.
 *
 * Usage:
 *   ./getpid_tree
 *
 * Skills practiced:
 *   - Reading /proc as a filesystem
 *   - opendir/readdir for directory listing
 *   - Parsing key: value files
 *   - Building a tree from flat parent pointers
 *   - Recursive printing with indentation
 *   - Handling processes that disappear during the scan (race)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>

#define MAX_PROCS  4096
#define NAME_MAX_LEN 256

typedef struct
{
    pid_t pid;
    pid_t ppid;
    char  comm[NAME_MAX_LEN];   /* process name */
    char  cmdline[NAME_MAX_LEN]; /* command line (first arg only) */
} proc_info;

static proc_info procs[MAX_PROCS];
static int nprocs = 0;

/* ---------- helpers ---------- */

/* Is the string all decimal digits? */
static int is_all_digits(const char *s)
{
    if (*s == '\0')
        return 0;
    for (const char *p = s; *p; p++)
        if (!isdigit((unsigned char)*p))
            return 0;
    return 1;
}

/* Read a "key: value" file and return the value for a given key,
 * copied into 'out'. Returns 1 on success, 0 otherwise. */
static int read_status_field(const char *pid_str, const char *key,
                             char *out, size_t out_size)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%s/status", pid_str);

    FILE *fp = fopen(path, "r");
    if (!fp)
        return 0;

    size_t keylen = strlen(key);
    char line[512];
    int found = 0;

    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, key, keylen) == 0 && line[keylen] == ':')
        {
            const char *value = line + keylen + 1;
            while (*value == ' ' || *value == '\t')
                value++;
            snprintf(out, out_size, "%s", value);
            /* Strip trailing newline */
            size_t vlen = strlen(out);
            while (vlen > 0 &&
                   (out[vlen - 1] == '\n' || out[vlen - 1] == '\r'))
                out[--vlen] = '\0';
            found = 1;
            break;
        }
    }

    fclose(fp);
    return found;
}

/* Read /proc/<pid>/cmdline. This is a NUL-separated list of
 * argv entries. We take the first one (the program name),
 * with the full path stripped to a basename for readability. */
static void read_cmdline(const char *pid_str, char *out, size_t out_size)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%s/cmdline", pid_str);

    FILE *fp = fopen(path, "rb");
    if (!fp)
    {
        snprintf(out, out_size, "?");
        return;
    }

    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);

    if (n == 0)
    {
        /* Kernel threads have empty cmdline */
        snprintf(out, out_size, "");
        return;
    }

    buf[n] = '\0';

    /* Take up to the first NUL */
    snprintf(out, out_size, "%s", buf);

    /* Strip path prefix if any */
    char *slash = strrchr(out, '/');
    if (slash)
        memmove(out, slash + 1, strlen(slash + 1) + 1);
}

/* Read /proc/<pid>/comm, which is the process name (no spaces,
 * max 15 chars). Fallback for when cmdline is empty. */
static void read_comm(const char *pid_str, char *out, size_t out_size)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%s/comm", pid_str);

    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        snprintf(out, out_size, "?");
        return;
    }

    if (!fgets(out, out_size, fp))
        out[0] = '\0';
    fclose(fp);

    size_t len = strlen(out);
    while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r'))
        out[--len] = '\0';
}

/* ---------- process scan ---------- */

static int scan_processes(void)
{
    DIR *dir = opendir("/proc");
    if (!dir)
    {
        perror("opendir /proc");
        return -1;
    }

    struct dirent *entry;
    nprocs = 0;

    while ((entry = readdir(dir)) != NULL && nprocs < MAX_PROCS)
    {
        if (!is_all_digits(entry->d_name))
            continue;

        pid_t pid = (pid_t)atoi(entry->d_name);

        char ppid_str[32];
        if (!read_status_field(entry->d_name, "PPid", ppid_str,
                               sizeof(ppid_str)))
        {
            /* Process might have exited between readdir and read */
            continue;
        }

        proc_info *p = &procs[nprocs];
        p->pid = pid;
        p->ppid = (pid_t)atoi(ppid_str);

        read_cmdline(entry->d_name, p->cmdline, sizeof(p->cmdline));
        read_comm(entry->d_name, p->comm, sizeof(p->comm));

        nprocs++;
    }

    closedir(dir);
    return 0;
}

/* Find a process by PID, or NULL if not in our snapshot */
static proc_info *find_proc(pid_t pid)
{
    for (int i = 0; i < nprocs; i++)
        if (procs[i].pid == pid)
            return &procs[i];
    return NULL;
}

/* Count how many processes have the given parent */
static int count_children(pid_t parent_pid)
{
    int count = 0;
    for (int i = 0; i < nprocs; i++)
        if (procs[i].ppid == parent_pid && procs[i].pid != parent_pid)
            count++;
    return count;
}

/* ---------- tree printing ---------- */

/* Print the tree rooted at 'pid', with the given prefix.
 * 'is_last' controls the tree-drawing characters on this branch. */
static void print_tree(pid_t pid, const char *prefix, int is_last,
                       int is_root)
{
    proc_info *p = find_proc(pid);

    if (is_root)
    {
        if (p)
            printf("%s [%d]\n",
                   p->cmdline[0] ? p->cmdline : p->comm, (int)p->pid);
        else
            printf("PID %d (not found in snapshot)\n", (int)pid);
    }
    else
    {
        printf("%s", prefix);
        printf("%s", is_last ? "\\-- " : "|-- ");
        if (p)
            printf("%s [%d]\n",
                   p->cmdline[0] ? p->cmdline : p->comm, (int)p->pid);
        else
            printf("(pid %d)\n", (int)pid);
    }

    /* Recurse on children */
    int children_remaining = count_children(pid);
    int seen = 0;

    for (int i = 0; i < nprocs; i++)
    {
        if (procs[i].ppid != pid || procs[i].pid == pid)
            continue;

        seen++;
        int child_is_last = (seen == children_remaining);

        /* Build the prefix for this child's subtree */
        char child_prefix[512];
        if (is_root)
            snprintf(child_prefix, sizeof(child_prefix), "%s", "");
        else
            snprintf(child_prefix, sizeof(child_prefix), "%s%s",
                     prefix, is_last ? "    " : "|   ");

        print_tree(procs[i].pid, child_prefix, child_is_last, 0);
    }
}

/* ---------- main ---------- */

int main(void)
{
    printf("====================================\n");
    printf("         PROCESS TREE               \n");
    printf("====================================\n\n");

    if (scan_processes() != 0)
        return 1;

    printf("Scanned %d processes from /proc\n\n", nprocs);

    /* Find PID 1 (init/systemd). Fall back to any process with
     * PPid 0 that we couldn't see children of. */
    if (find_proc(1))
    {
        print_tree(1, "", 1, 1);
    }
    else
    {
        printf("PID 1 not found in snapshot. Showing all roots:\n\n");
        for (int i = 0; i < nprocs; i++)
        {
            if (procs[i].ppid == 0 ||
                !find_proc(procs[i].ppid))
            {
                print_tree(procs[i].pid, "", 1, 1);
            }
        }
    }

    printf("\nTotal: %d processes\n", nprocs);
    return 0;
}

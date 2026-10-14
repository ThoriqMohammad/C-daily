/* mini_ls.c
 *
 * Lists the contents of a directory using opendir() and readdir().
 *
 * For each entry in the given directory, this tool prints:
 *   - a type indicator (d for directory, l for symlink, - for regular)
 *   - the permission bits in rwx form
 *   - the size in bytes
 *   - the entry name
 *
 * This is a miniature version of the standard `ls -l` command.
 *
 * Usage:
 *   ./mini_ls
 *   ./mini_ls /etc
 *   ./mini_ls ..
 *
 * Skills practiced:
 *   - Directory streams with opendir(), readdir(), closedir()
 *   - struct dirent and d_name
 *   - stat() on each entry to fetch metadata
 *   - snprintf() to build full paths safely
 *   - Bitmasking with st_mode (reused from day 33)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PATH_MAX_LEN 4096

/* Build the rwx permission string (e.g. "-rw-r--r--") */
void permission_string(mode_t mode, char *out)
{
    out[0] = S_ISDIR(mode)  ? 'd' :
             S_ISLNK(mode)  ? 'l' :
             S_ISCHR(mode)  ? 'c' :
             S_ISBLK(mode)  ? 'b' :
             S_ISFIFO(mode) ? 'p' :
             S_ISSOCK(mode) ? 's' : '-';

    out[1] = (mode & S_IRUSR) ? 'r' : '-';
    out[2] = (mode & S_IWUSR) ? 'w' : '-';
    out[3] = (mode & S_IXUSR) ? 'x' : '-';
    out[4] = (mode & S_IRGRP) ? 'r' : '-';
    out[5] = (mode & S_IWGRP) ? 'w' : '-';
    out[6] = (mode & S_IXGRP) ? 'x' : '-';
    out[7] = (mode & S_IROTH) ? 'r' : '-';
    out[8] = (mode & S_IWOTH) ? 'w' : '-';
    out[9] = (mode & S_IXOTH) ? 'x' : '-';
    out[10] = '\0';
}

/* Print usage */
void usage(const char *program)
{
    fprintf(stderr, "Usage: %s [directory]\n", program);
    fprintf(stderr, "  directory: path to list (default: \".\")\n");
}

int main(int argc, char *argv[])
{
    if (argc > 2)
    {
        usage(argv[0]);
        return 1;
    }

    const char *dirpath = (argc == 2) ? argv[1] : ".";

    DIR *dir = opendir(dirpath);
    if (dir == NULL)
    {
        perror("opendir");
        return 1;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        /* Skip the . and .. entries for cleaner output */
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char fullpath[PATH_MAX_LEN];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);

        struct stat sb;
        if (lstat(fullpath, &sb) == -1)
        {
            /* Report the failure but keep listing the rest */
            perror(entry->d_name);
            continue;
        }

        char perms[11];
        permission_string(sb.st_mode, perms);

        printf("%s %8lld  %s\n",
               perms,
               (long long)sb.st_size,
               entry->d_name);
    }

    closedir(dir);
    return 0;
}

/* file_stats.c
 *
 * Prints metadata about a file using the stat() syscall.
 *
 * For a given path, this tool reports:
 *   - the file type (regular, directory, symlink, etc.)
 *   - the size in bytes
 *   - the permission bits (both rwx string and octal)
 *   - the inode number
 *   - the number of hard links
 *   - the owner and group IDs
 *   - the last modification time
 *
 * This is a miniature version of the standard `stat` command.
 *
 * Usage:
 *   ./file_stats README.md
 *   ./file_stats /etc/passwd
 *
 * Skills practiced:
 *   - The stat() syscall and struct stat
 *   - Bitmasking with st_mode (S_ISREG, S_ISDIR, S_IRUSR, ...)
 *   - Octal output with printf("%o", ...)
 *   - Time formatting with localtime() and strftime()
 *   - Error handling with perror()
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* Print the file type from st_mode as a word */
const char *file_type(mode_t mode)
{
    if (S_ISREG(mode))  return "regular file";
    if (S_ISDIR(mode))  return "directory";
    if (S_ISLNK(mode))  return "symbolic link";
    if (S_ISCHR(mode))  return "character device";
    if (S_ISBLK(mode))  return "block device";
    if (S_ISFIFO(mode)) return "FIFO (named pipe)";
    if (S_ISSOCK(mode)) return "socket";
    return "unknown";
}

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
    fprintf(stderr, "Usage: %s <path>\n", program);
    fprintf(stderr, "  path: file or directory to inspect\n");
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        usage(argv[0]);
        return 1;
    }

    const char *path = argv[1];
    struct stat sb;

    if (stat(path, &sb) == -1)
    {
        perror("stat");
        return 1;
    }

    char perms[11];
    permission_string(sb.st_mode, perms);

    char timebuf[64];
    struct tm *tm_info = localtime(&sb.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("File:        %s\n", path);
    printf("Type:        %s\n", file_type(sb.st_mode));
    printf("Size:        %lld bytes\n", (long long)sb.st_size);
    printf("Permissions: %s (%04o)\n", perms, sb.st_mode & 07777);
    printf("Inode:       %llu\n", (unsigned long long)sb.st_ino);
    printf("Links:       %llu\n", (unsigned long long)sb.st_nlink);
    printf("Owner:       uid=%u\n", (unsigned)sb.st_uid);
    printf("Group:       gid=%u\n", (unsigned)sb.st_gid);
    printf("Modified:    %s\n", timebuf);

    return 0;
}

/* mmap_demo.c
 *
 * Demonstrates memory mapping a file with mmap(), msync(),
 * and munmap().
 *
 * mmap() asks the kernel to attach a file (or anonymous memory)
 * directly into the process's address space. Reading from or
 * writing to that address range becomes ordinary memory access;
 * no read() or write() syscalls are needed per byte.
 *
 * Why this matters:
 *   - It is how the dynamic loader maps shared libraries.
 *   - It is how databases read files without copying through
 *     user buffers (mmap + page cache = zero-copy I/O).
 *   - It is how shared memory works between processes.
 *   - It is how large files can be edited without loading them
 *     entirely into RAM.
 *
 * The program walks through four small experiments:
 *
 *   1. Create a file, mmap it writable, write through the pointer.
 *   2. munmap() and verify the file on disk has the data.
 *   3. Re-open, mmap read-only, read through the pointer.
 *   4. mmap anonymous memory (MAP_ANONYMOUS) for scratch use.
 *
 * Usage:
 *   ./mmap_demo
 *
 * Skills practiced:
 *   - open() with O_RDWR / O_CREAT / O_TRUNC
 *   - ftruncate() to give the file a size
 *   - mmap() with MAP_SHARED for file-backed memory
 *   - msync() to flush dirty pages to disk
 *   - munmap() to detach
 *   - mmap() with MAP_ANONYMOUS | MAP_PRIVATE for scratch
 *   - Error handling for each step
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DEMO_FILE "mmap_demo_out.txt"
#define DEMO_SIZE 4096

/* Demo 1: create a file, map it writable, write through the pointer */
static void demo_write_mapping(void)
{
    printf("--- 1. Write through a file-backed mmap ---\n");

    int fd = open(DEMO_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    /* A file must be at least as long as the mapping, or writes
     * past EOF will SIGBUS. ftruncate() allocates the space. */
    if (ftruncate(fd, DEMO_SIZE) == -1)
    {
        perror("ftruncate");
        close(fd);
        exit(EXIT_FAILURE);
    }

    void *addr = mmap(NULL, DEMO_SIZE,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    /* From here on, the mapping behaves like an ordinary
     * character buffer. No read() / write() needed. */
    char *p = addr;
    snprintf(p, DEMO_SIZE,
             "Written via mmap at 0x%p\n",
             addr);

    printf("Mapped %d bytes at %p, wrote a string through it.\n",
           DEMO_SIZE, addr);

    /* msync flushes dirty pages to disk. Without MAP_SYNC this
     * is technically optional before munmap, but it makes the
     * demo deterministic. */
    if (msync(addr, DEMO_SIZE, MS_SYNC) == -1)
        perror("msync");

    if (munmap(addr, DEMO_SIZE) == -1)
        perror("munmap");

    close(fd);
    printf("Unmapped and closed.\n\n");
}

/* Demo 2: re-open read-only, map read-only, read through pointer */
static void demo_read_mapping(void)
{
    printf("--- 2. Read back via a read-only mmap ---\n");

    int fd = open(DEMO_FILE, O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        perror("fstat");
        close(fd);
        exit(EXIT_FAILURE);
    }

    size_t len = (size_t)sb.st_size;

    void *addr = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("File contents (%zu bytes):\n  ", len);

    const char *p = addr;
    for (size_t i = 0; i < len && p[i] != '\0'; i++)
        putchar(p[i]);

    if (munmap(addr, len) == -1)
        perror("munmap");

    close(fd);
    printf("\n\n");
}

/* Demo 3: anonymous mapping, for scratch memory */
static void demo_anonymous(void)
{
    printf("--- 3. Anonymous mapping for scratch memory ---\n");

    size_t len = 256;

    void *addr = mmap(NULL, len,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED)
    {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    char *p = addr;
    strcpy(p, "anonymous memory feels like malloc but at page granularity");
    printf("Wrote: \"%s\"\n", p);
    printf("Mapped at: %p\n", addr);

    if (munmap(addr, len) == -1)
        perror("munmap");

    printf("Unmapped.\n\n");
}

/* Demo 4: show that write-through works - two mappings of the same file */
static void demo_shared_view(void)
{
    printf("--- 4. Two mappings of the same file see each other ---\n");

    int fd = open(DEMO_FILE, O_RDWR);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        perror("fstat");
        close(fd);
        exit(EXIT_FAILURE);
    }

    size_t len = (size_t)sb.st_size;

    char *a = mmap(NULL, len, PROT_READ | PROT_WRITE,
                   MAP_SHARED, fd, 0);
    char *b = mmap(NULL, len, PROT_READ | PROT_WRITE,
                   MAP_SHARED, fd, 0);

    if (a == MAP_FAILED || b == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("Before: a[0] = '%c', b[0] = '%c'\n", a[0], b[0]);

    /* Write through one mapping; the other sees the change
     * immediately, because MAP_SHARED points both mappings at
     * the same physical page in the page cache. */
    a[0] = 'X';

    printf("After:  a[0] = '%c', b[0] = '%c'  (changed only through a)\n",
           a[0], b[0]);

    munmap(a, len);
    munmap(b, len);
    close(fd);
    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("            MMAP DEMO               \n");
    printf("====================================\n\n");

    demo_write_mapping();
    demo_read_mapping();
    demo_anonymous();
    demo_shared_view();

    printf("Done. %s was written during the demo.\n", DEMO_FILE);
    return 0;
}

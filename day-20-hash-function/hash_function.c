/* hash_function.c
 *
 * Demonstrates several classic hash functions for strings.
 *
 * A hash function maps data of arbitrary size to a fixed-size value.
 * Hash functions are used in:
 *   - Hash tables
 *   - Checksums
 *   - Data integrity
 *   - Password storage (with care)
 *
 * This program implements:
 *   - djb2 (Dan Bernstein)
 *   - FNV-1a (Fowler–Noll–Vo)
 *   - sdbm
 *   - A simple checksum
 *
 * Usage:
 *   ./hash_function <string>
 *
 * Example:
 *   ./hash_function hello
 *
 * Skills practiced:
 *   - Bitwise operations
 *   - Modular arithmetic
 *   - Unsigned integer overflow (defined behavior)
 *   - Hashing concepts
 *   - argc / argv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* djb2 — Dan Bernstein's classic hash */
uint32_t djb2(const char *str)
{
    uint32_t hash = 5381;
    int c;

    while ((c = (unsigned char) *str++))
        hash = ((hash << 5) + hash) + c;   /* hash * 33 + c */

    return hash;
}

/* FNV-1a — Fowler–Noll–Vo hash */
uint32_t fnv1a(const char *str)
{
    uint32_t hash = 2166136261u;   /* FNV offset basis */

    for (const unsigned char *p = (const unsigned char *) str; *p; p++)
    {
        hash ^= *p;
        hash *= 16777619u;          /* FNV prime */
    }

    return hash;
}

/* sdbm — used in Berkeley DB */
uint32_t sdbm(const char *str)
{
    uint32_t hash = 0;
    int c;

    while ((c = (unsigned char) *str++))
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

/* A simple additive checksum — not a good hash, but illustrative */
uint32_t simple_sum(const char *str)
{
    uint32_t sum = 0;

    for (const unsigned char *p = (const unsigned char *) str; *p; p++)
        sum += *p;

    return sum;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <string>\n", argv[0]);
        return 1;
    }

    const char *s = argv[1];

    printf("====================================\n");
    printf("        HASH FUNCTION DEMO          \n");
    printf("====================================\n\n");

    printf("Input: \"%s\"\n", s);
    printf("Length: %zu\n\n", strlen(s));

    printf("djb2:       %10u  (0x%08X)\n", djb2(s), djb2(s));
    printf("FNV-1a:     %10u  (0x%08X)\n", fnv1a(s), fnv1a(s));
    printf("sdbm:       %10u  (0x%08X)\n", sdbm(s), sdbm(s));
    printf("simple_sum: %10u  (0x%08X)\n", simple_sum(s), simple_sum(s));

    return 0;
}

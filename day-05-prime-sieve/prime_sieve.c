/* prime_sieve.c
 *
 * Finds all prime numbers up to a given limit using the
 * Sieve of Eratosthenes.
 *
 * The Sieve of Eratosthenes is one of the oldest and most
 * efficient algorithms for finding all primes up to n.
 *
 * Usage:
 *   ./prime_sieve n
 *
 * Example:
 *   ./prime_sieve 50
 *   Primes up to 50:
 *   2 3 5 7 11 13 17 19 23 29 31 37 41 43 47
 *   Total: 15 primes
 *
 * Skills practiced:
 *   - Arrays
 *   - Nested loops
 *   - Number theory
 *   - argc/argv
 *   - atoi
 *   - Algorithmic thinking (sieve)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mark all composite numbers up to n */
void sieve(int n, int is_prime[])
{
    /* Start by assuming every number is prime */
    for (int i = 0; i <= n; i++)
        is_prime[i] = 1;

    is_prime[0] = 0;
    is_prime[1] = 0;

    /* Mark multiples of each prime as composite */
    for (int p = 2; p * p <= n; p++)
    {
        if (is_prime[p])
        {
            for (int multiple = p * p; multiple <= n; multiple += p)
                is_prime[multiple] = 0;
        }
    }
}

/* Count how many primes are in the array */
int count_primes(int n, int is_prime[])
{
    int count = 0;
    for (int i = 0; i <= n; i++)
        if (is_prime[i])
            count++;
    return count;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s n\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);

    if (n < 2)
    {
        fprintf(stderr, "Error: n must be at least 2.\n");
        return 1;
    }

    /* Allocate array on the heap (safe for large n) */
    int *is_prime = malloc((n + 1) * sizeof(int));
    if (is_prime == NULL)
    {
        fprintf(stderr, "Error: memory allocation failed.\n");
        return 1;
    }

    sieve(n, is_prime);

    printf("Primes up to %d:\n", n);
    for (int i = 2; i <= n; i++)
    {
        if (is_prime[i])
            printf("%d ", i);
    }
    printf("\n");

    printf("Total: %d primes\n", count_primes(n, is_prime));

    free(is_prime);
    return 0;
}

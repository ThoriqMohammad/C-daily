/* fibonacci_memo.c
 *
 * Computes Fibonacci numbers using memoization.
 *
 * The naive recursive Fibonacci is O(2^n)—painfully slow.
 * Memoization stores computed values, reducing it to O(n).
 *
 * This program compares:
 *   - Naive recursion
 *   - Memoized recursion
 *   - Iterative (bottom-up)
 *
 * Usage:
 *   ./fibonacci_memo n
 *
 * Example:
 *   ./fibonacci_memo 40
 *
 * Skills practiced:
 *   - Dynamic programming (memoization)
 *   - Recursion
 *   - Arrays
 *   - Time complexity (O(2^n) vs. O(n))
 *   - argc / argv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_N 90

/* Naive recursive Fibonacci — O(2^n) */
long long fib_naive(int n)
{
    if (n <= 1)
        return n;
    return fib_naive(n - 1) + fib_naive(n - 2);
}

/* Memoized recursive Fibonacci — O(n) */
long long fib_memo(int n, long long memo[])
{
    if (n <= 1)
        return n;

    if (memo[n] != -1)
        return memo[n];

    memo[n] = fib_memo(n - 1, memo) + fib_memo(n - 2, memo);
    return memo[n];
}

/* Iterative Fibonacci — O(n), O(1) space */
long long fib_iter(int n)
{
    if (n <= 1)
        return n;

    long long a = 0, b = 1, c;

    for (int i = 2; i <= n; i++)
    {
        c = a + b;
        a = b;
        b = c;
    }

    return b;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s n\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);

    if (n < 0 || n > MAX_N)
    {
        fprintf(stderr, "Error: n must be between 0 and %d.\n", MAX_N);
        return 1;
    }

    printf("Fibonacci(%d)\n", n);
    printf("================\n\n");

    /* Iterative */
    clock_t start = clock();
    long long result_iter = fib_iter(n);
    clock_t end = clock();
    double time_iter = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Iterative:  %lld  (%.6f s)\n", result_iter, time_iter);

    /* Memoized */
    long long *memo = malloc((n + 1) * sizeof(long long));
    if (memo == NULL)
    {
        fprintf(stderr, "Error: malloc failed.\n");
        return 1;
    }

    for (int i = 0; i <= n; i++)
        memo[i] = -1;

    start = clock();
    long long result_memo = fib_memo(n, memo);
    end = clock();
    double time_memo = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Memoized:   %lld  (%.6f s)\n", result_memo, time_memo);

    free(memo);

    /* Naive — only for small n */
    if (n <= 40)
    {
        start = clock();
        long long result_naive = fib_naive(n);
        end = clock();
        double time_naive = (double)(end - start) / CLOCKS_PER_SEC;

        printf("Naive:      %lld  (%.6f s)\n", result_naive, time_naive);
    }
    else
    {
        printf("Naive:      skipped (too slow for n > 40)\n");
    }

    return 0;
}

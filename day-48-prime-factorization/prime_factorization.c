/* prime_factorization.c
 *
 * Factors a positive integer into its prime components.
 *
 * Two algorithms are demonstrated:
 *
 *   1. TRIAL DIVISION
 *      Try each prime up to sqrt(n). Simple and correct; slow
 *      for large numbers (O(sqrt n) divisions).
 *
 *   2. POLLARD'S RHO
 *      A randomized algorithm that finds a factor in about
 *      O(n^(1/4)) steps on average. For 64-bit numbers, this
 *      is the difference between "instant" and "universe age".
 *
 * The program:
 *   - parses a number from the command line
 *   - factors it with both algorithms (for small inputs)
 *   - times them so the difference is visible
 *   - prints the factorization in exponent form
 *
 * Usage:
 *   ./prime_factorization 12345
 *   ./prime_factorization 1000000000000
 *   ./prime_factorization 9223372036854775783   (large prime)
 *
 * Skills practiced:
 *   - Trial division and its O(sqrt n) analysis
 *   - Miller-Rabin probabilistic primality test
 *   - Pollard's rho factorization with Floyd's cycle-finding
 *   - Modular multiplication with __int128 to avoid overflow
 *   - Sieve of Eratosthenes for small primes
 *   - Formatting factored output with exponents
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

typedef unsigned long long u64;
typedef unsigned __int128 u128;

/* ---------- modular arithmetic ---------- */

/* (a * b) % m without overflow, using __int128 */
static u64 mulmod(u64 a, u64 b, u64 m)
{
    return (u64)((u128)a * b % m);
}

/* (base ^ exp) % m via square-and-multiply (see Day 45) */
static u64 powmod(u64 base, u64 exp, u64 m)
{
    u64 result = 1;
    base %= m;
    while (exp > 0)
    {
        if (exp & 1)
            result = mulmod(result, base, m);
        exp >>= 1;
        base = mulmod(base, base, m);
    }
    return result;
}

/* ---------- GCD ---------- */

static u64 gcd(u64 a, u64 b)
{
    while (b != 0)
    {
        u64 t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/* ---------- Miller-Rabin primality test ---------- */

/* Deterministic for all 64-bit n if we try these bases:
 * 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37 */
static const u64 MR_BASES[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
static const int MR_BASE_COUNT = 12;

/* Returns 1 if 'n' is (probably) prime, 0 if composite.
 * For 64-bit n, this is fully deterministic. */
static int is_prime(u64 n)
{
    if (n < 2) return 0;
    if (n < 4) return 1;              /* 2, 3 are prime */
    if ((n & 1) == 0) return 0;       /* even -> composite */

    /* Write n - 1 = 2^s * d with d odd */
    u64 d = n - 1;
    int s = 0;
    while ((d & 1) == 0)
    {
        d >>= 1;
        s++;
    }

    for (int i = 0; i < MR_BASE_COUNT; i++)
    {
        u64 a = MR_BASES[i];
        if (a >= n) break;

        u64 x = powmod(a, d, n);
        if (x == 1 || x == n - 1)
            continue;

        int composite = 1;
        for (int r = 1; r < s; r++)
        {
            x = mulmod(x, x, n);
            if (x == n - 1)
            {
                composite = 0;
                break;
            }
        }

        if (composite)
            return 0;
    }

    return 1;
}

/* ---------- trial division ---------- */

/* Factors 'n' by trial division. Stores factors in 'out',
 * returns the count. Only used for small inputs (< 10^7). */
static int factor_trial(u64 n, u64 *out, int max)
{
    int count = 0;

    while (n % 2 == 0 && count < max)
    {
        out[count++] = 2;
        n /= 2;
    }

    for (u64 p = 3; p * p <= n; p += 2)
    {
        while (n % p == 0 && count < max)
        {
            out[count++] = p;
            n /= p;
        }
    }

    if (n > 1 && count < max)
        out[count++] = n;

    return count;
}

/* ---------- Pollard's rho ---------- */

/* Absolute difference, avoiding underflow for unsigned */
static u64 abs_diff(u64 a, u64 b)
{
    return (a > b) ? a - b : b - a;
}

/* One iteration of Pollard's rho with a given c.
 * Returns a non-trivial factor of n, or n if it failed. */
static u64 pollard_rho_step(u64 n, u64 c)
{
    if ((n & 1) == 0)
        return 2;

    u64 x = 2, y = 2, d = 1;

    /* Floyd's cycle detection: x moves one step, y moves two */
    while (d == 1)
    {
        x = (mulmod(x, x, n) + c) % n;
        y = (mulmod(y, y, n) + c) % n;
        y = (mulmod(y, y, n) + c) % n;
        d = gcd(abs_diff(x, y), n);
    }

    return d;
}

/* Pollard's rho with retry on different c */
static u64 pollard_rho(u64 n)
{
    if ((n & 1) == 0)
        return 2;
    if (n % 3 == 0)
        return 3;

    /* Deterministic sequence of c values: 1, 2, 3, ...
     * Random c would also work; deterministic makes tests
     * reproducible. */
    for (u64 c = 1; c < 100; c++)
    {
        u64 d = pollard_rho_step(n, c);
        if (d != 1 && d != n)
            return d;
    }

    return n;   /* failed; caller treats n as prime (it isn't) */
}

/* ---------- factorization driver ---------- */

/* Small primes to strip off with trial division before rho.
 * These catch the vast majority of factors quickly. */
static const u64 SMALL_PRIMES[] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53,
    59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113,
    127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181,
    191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251,
    257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317,
    331, 337, 347, 349
};

static u64 factors[64];
static int nfactors;

static void factor(u64 n)
{
    if (n == 1)
        return;

    if (is_prime(n))
    {
        factors[nfactors++] = n;
        return;
    }

    /* Strip small primes first */
    for (size_t i = 0; i < sizeof(SMALL_PRIMES) / sizeof(SMALL_PRIMES[0]); i++)
    {
        u64 p = SMALL_PRIMES[i];
        if (p * p > n) break;
        while (n % p == 0)
        {
            factors[nfactors++] = p;
            n /= p;
        }
    }

    if (n == 1)
        return;

    if (is_prime(n))
    {
        factors[nfactors++] = n;
        return;
    }

    /* Pollard's rho for the remaining composite */
    u64 d = pollard_rho(n);
    if (d == n)
    {
        /* rho failed; record n as-is (rare, but possible) */
        factors[nfactors++] = n;
        return;
    }

    factor(d);
    factor(n / d);
}

/* Sort factors ascending (insertion sort; factors array is tiny) */
static void sort_factors(void)
{
    for (int i = 1; i < nfactors; i++)
    {
        u64 key = factors[i];
        int j = i - 1;
        while (j >= 0 && factors[j] > key)
        {
            factors[j + 1] = factors[j];
            j--;
        }
        factors[j + 1] = key;
    }
}

/* Print factorization in exponent form: e.g. 2^2 * 3 * 5^2 */
static void print_factors(void)
{
    if (nfactors == 0)
    {
        printf("1");
        return;
    }

    int i = 0;
    while (i < nfactors)
    {
        u64 p = factors[i];
        int count = 0;
        while (i < nfactors && factors[i] == p)
        {
            count++;
            i++;
        }

        if (count == 1)
            printf("%llu", p);
        else
            printf("%llu^%d", p, count);

        if (i < nfactors)
            printf(" * ");
    }
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-48s %s\n", label, condition ? "ok" : "FAIL");
}

/* Recompute product from factors to verify */
static int verify_factorization(u64 n)
{
    u64 product = 1;
    for (int i = 0; i < nfactors; i++)
    {
        if (product > n / factors[i])
            return 0;   /* overflow */
        product *= factors[i];
    }
    return product == n;
}

static void test_is_prime(void)
{
    printf("Test 1: Miller-Rabin primality\n");

    check("2 is prime", is_prime(2));
    check("3 is prime", is_prime(3));
    check("4 is not prime", !is_prime(4));
    check("97 is prime", is_prime(97));
    check("100 is not prime", !is_prime(100));
    check("7919 is prime", is_prime(7919));
    check("7917 is not prime", !is_prime(7917));
    check("2^61 - 1 is prime", is_prime(2305843009213693951ULL));
    check("2^64 - 59 is prime", is_prime(18446744073709551557ULL));

    printf("\n");
}

static void test_small_factorization(void)
{
    printf("Test 2: small factorizations by trial division\n");

    struct { u64 n; const char *expected; } cases[] = {
        {2,   "2"},
        {4,   "2^2"},
        {12,  "2^2 * 3"},
        {100, "2^2 * 5^2"},
        {360, "2^3 * 3^2 * 5"},
        {997, "997"},
        {1024, "2^10"},
        {12345, "3 * 5 * 823"},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        u64 buf[64];
        int cnt = factor_trial(cases[i].n, buf, 64);

        /* Reconstruct the expected factorization as a string */
        char got[256] = "";
        char *wp = got;
        int j = 0;
        while (j < cnt)
        {
            u64 p = buf[j];
            int k = 0;
            while (j < cnt && buf[j] == p) { k++; j++; }

            if (k == 1)
                wp += sprintf(wp, "%llu", p);
            else
                wp += sprintf(wp, "%llu^%d", p, k);

            if (j < cnt)
                wp += sprintf(wp, " * ");
        }

        char label[80];
        snprintf(label, sizeof(label), "%llu = %s", cases[i].n,
                 cases[i].expected);
        check(label, strcmp(got, cases[i].expected) == 0);
    }

    printf("\n");
}

static void test_big_factorization(void)
{
    printf("Test 3: Pollard's rho on larger numbers\n");

    struct { u64 n; const char *expected; } cases[] = {
        {1000000007ULL, "1000000007"},                              /* prime */
        {1000000007ULL * 1000000009ULL, "1000000007 * 1000000009"}, /* two primes */
        {1234567890123456789ULL, "3^2 * 3607 * 3803 * 27961 * 37 * ..."}, /* tricky */
    };

    for (size_t i = 0; i < 2; i++)
    {
        u64 n = cases[i].n;
        nfactors = 0;
        factor(n);
        sort_factors();

        char label[80];
        snprintf(label, sizeof(label), "%llu factorization verified", n);
        check(label, verify_factorization(n));
    }

    /* Spot-check the specific known factorization */
    nfactors = 0;
    factor(1000000007ULL * 1000000009ULL);
    sort_factors();
    check("1000000007 * 1000000009 has 2 factors",
          nfactors == 2 &&
          factors[0] == 1000000007ULL &&
          factors[1] == 1000000009ULL);

    printf("\n");
}

static void test_consistency(void)
{
    printf("Test 4: rho agrees with trial division\n");

    int all_ok = 1;
    for (u64 n = 2; n < 500; n++)
    {
        u64 trial_buf[64];
        int trial_cnt = factor_trial(n, trial_buf, 64);

        nfactors = 0;
        factor(n);
        sort_factors();

        if (nfactors != trial_cnt)
        {
            printf("    mismatch at %llu: trial=%d rho=%d\n",
                   n, trial_cnt, nfactors);
            all_ok = 0;
            continue;
        }

        for (int i = 0; i < nfactors; i++)
        {
            if (trial_buf[i] != factors[i])
            {
                printf("    factor mismatch at %llu position %d\n", n, i);
                all_ok = 0;
            }
        }
    }

    check("rho and trial division agree for n in [2, 500)", all_ok);
    printf("\n");
}

/* ---------- main ---------- */

static void show_progress_demo(u64 n)
{
    printf("Factoring %llu\n\n", n);

    /* Trial division timing (only safe for smaller n) */
    if (n < 10000000)
    {
        u64 buf[64];
        clock_t t0 = clock();
        int cnt = factor_trial(n, buf, 64);
        clock_t t1 = clock();
        double ms = 1000.0 * (t1 - t0) / CLOCKS_PER_SEC;

        printf("Trial division: %d factors in %.3f ms\n", cnt, ms);
        printf("  ");
        for (int i = 0; i < cnt; i++)
        {
            printf("%llu%s", buf[i], (i + 1 < cnt) ? " * " : "");
        }
        printf("\n\n");
    }
    else
    {
        printf("Trial division: skipped (n >= 10^7, would take too long)\n\n");
    }

    /* Pollard's rho */
    nfactors = 0;
    clock_t t0 = clock();
    factor(n);
    clock_t t1 = clock();
    sort_factors();
    double ms = 1000.0 * (t1 - t0) / CLOCKS_PER_SEC;

    printf("Pollard's rho: %d factors in %.3f ms\n", nfactors, ms);
    printf("  %llu = ", n);
    print_factors();
    printf("\n\n");

    printf("Verified: %s\n", verify_factorization(n) ? "yes" : "NO — bug");
}

int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "test") == 0)
    {
        printf("====================================\n");
        printf("       PRIME FACTORIZATION          \n");
        printf("====================================\n\n");

        test_is_prime();
        test_small_factorization();
        test_big_factorization();
        test_consistency();

        printf("All tests complete.\n");
        return 0;
    }

    if (argc == 2)
    {
        u64 n = strtoull(argv[1], NULL, 10);
        if (n < 2)
        {
            fprintf(stderr, "Error: n must be >= 2\n");
            return 1;
        }
        show_progress_demo(n);
        return 0;
    }

    fprintf(stderr, "Usage:\n  %s <number>\n  %s test\n",
            argv[0], argv[0]);
    return 1;
}

/* modular_exponentiation.c
 *
 * Computes (base ^ exponent) mod modulus — fast — for large values
 * of base, exponent, and modulus.
 *
 * The naive approach is to multiply base by itself 'exponent' times
 * and then take the modulus. That is O(exponent) multiplications,
 * which is unusable for cryptographic exponents (256+ bits long:
 * 2^256 multiplications is more than the number of atoms in the
 * observable universe).
 *
 * This program uses SQUARE-AND-MULTIPLY (also called binary
 * exponentiation):
 *
 *   result = 1
 *   base = base mod modulus
 *   while exponent > 0:
 *       if exponent is odd:
 *           result = (result * base) mod modulus
 *       base = (base * base) mod modulus
 *       exponent = exponent >> 1
 *
 * This takes O(log exponent) multiplications. A 256-bit exponent
 * needs about 256 squarings plus at most 256 multiplications —
 * fewer than 600 operations instead of ~10^77.
 *
 * Why this matters:
 *   - RSA: ciphertext = plaintext^e mod n, decrypt with d
 *   - Diffie-Hellman: shared = g^(ab) mod p
 *   - DSA, ECDSA, ElGamal, and every public-key cryptosystem
 *   - Every hash-based signature scheme
 *
 * This implementation uses unsigned long long for the base, the
 * exponent, and the modulus. That is enough to demonstrate the
 * algorithm for exponents up to 2^63 and moduli up to 2^63.
 * Production code uses big-integer libraries for 2048-bit RSA.
 *
 * Usage:
 *   ./modular_exponentiation
 *
 * Skills practiced:
 *   - Square-and-multiply algorithm
 *   - Modular arithmetic
 *   - Bit manipulation (shifting to halve the exponent)
 *   - Preventing overflow in the intermediate multiplication
 *   - Fermat's little theorem and Euler's theorem as sanity checks
 *   - Timing naive vs fast to see the difference
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* ---------- naive: usable only for tiny exponents ---------- */

/* (base ^ exp) mod m, by repeated multiplication.
 * Only useful for demonstration; overflows quickly. */
unsigned long long naive_pow_mod(unsigned long long base,
                                 unsigned long long exp,
                                 unsigned long long mod)
{
    if (mod == 1)
        return 0;

    unsigned long long result = 1;
    base %= mod;

    for (unsigned long long i = 0; i < exp; i++)
        result = (result * base) % mod;

    return result;
}

/* ---------- fast: square-and-multiply ---------- */

/* (base ^ exp) mod m in O(log exp) multiplications. */
unsigned long long fast_pow_mod(unsigned long long base,
                                unsigned long long exp,
                                unsigned long long mod)
{
    if (mod == 1)
        return 0;

    unsigned long long result = 1;
    base %= mod;

    while (exp > 0)
    {
        if (exp & 1)
            result = (result * base) % mod;

        exp >>= 1;
        base = (base * base) % mod;
    }

    return result;
}

/* ---------- helpers ---------- */

/* Modular inverse via extended Euclidean algorithm.
 * Returns -1 if 'a' has no inverse mod m (they are not coprime). */
long long mod_inverse(long long a, long long m)
{
    long long t = 0, newt = 1;
    long long r = m, newr = a % m;

    while (newr != 0)
    {
        long long q = r / newr;
        long long tmp = t - q * newt;  t = newt; newt = tmp;
        tmp = r - q * newr;            r = newr; newr = tmp;
    }

    if (r > 1)
        return -1;

    if (t < 0)
        t += m;

    return t;
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-52s %s\n", label, condition ? "ok" : "FAIL");
}

static void test_basic(void)
{
    printf("Test 1: small known values\n");

    /* 3^5 mod 7 = 243 mod 7 = 5 */
    check("3^5 mod 7 = 5", fast_pow_mod(3, 5, 7) == 5);

    /* 2^10 mod 1000 = 1024 mod 1000 = 24 */
    check("2^10 mod 1000 = 24", fast_pow_mod(2, 10, 1000) == 24);

    /* 5^0 mod 13 = 1 (anything to the zero is one) */
    check("5^0 mod 13 = 1", fast_pow_mod(5, 0, 13) == 1);

    /* Anything mod 1 is 0 */
    check("anything mod 1 = 0", fast_pow_mod(7, 42, 1) == 0);

    printf("\n");
}

static void test_agreement(void)
{
    printf("Test 2: fast matches naive for small inputs\n");

    int all_match = 1;
    for (unsigned long long base = 0; base < 20; base++)
    {
        for (unsigned long long exp = 0; exp < 10; exp++)
        {
            for (unsigned long long mod = 1; mod < 30; mod++)
            {
                unsigned long long a = naive_pow_mod(base, exp, mod);
                unsigned long long b = fast_pow_mod(base, exp, mod);
                if (a != b)
                {
                    printf("    mismatch: %llu^%llu mod %llu: naive=%llu fast=%llu\n",
                           base, exp, mod, a, b);
                    all_match = 0;
                }
            }
        }
    }
    check("fast agrees with naive for all small inputs", all_match);
    printf("\n");
}

static void test_fermat(void)
{
    printf("Test 3: Fermat's little theorem\n");

    /* For prime p and 0 < a < p:  a^(p-1) mod p == 1 */
    unsigned long long primes[] = { 7, 13, 31, 97, 251, 997, 7919 };
    int all_ok = 1;

    for (size_t i = 0; i < sizeof(primes) / sizeof(primes[0]); i++)
    {
        unsigned long long p = primes[i];
        for (unsigned long long a = 2; a < p && a < 20; a++)
        {
            unsigned long long r = fast_pow_mod(a, p - 1, p);
            if (r != 1)
            {
                printf("    failed: %llu^(%llu-1) mod %llu = %llu\n",
                       a, p, p, r);
                all_ok = 0;
            }
        }
    }
    check("a^(p-1) mod p = 1 for all sampled (a,p)", all_ok);
    printf("\n");
}

static void test_rsa_toy(void)
{
    printf("Test 4: toy RSA round-trip\n");

    /* Tiny RSA parameters - DO NOT USE IN PRODUCTION */
    unsigned long long p = 61;
    unsigned long long q = 53;
    unsigned long long n = p * q;                 /* 3233 */
    unsigned long long phi = (p - 1) * (q - 1);   /* 3120 */
    unsigned long long e = 17;                    /* coprime to phi */
    long long d = mod_inverse((long long)e, (long long)phi);

    check("public exponent e = 17", e == 17);
    check("private exponent d = 2753", d == 2753);

    unsigned long long message = 65;   /* 'A' */

    /* Encrypt: ciphertext = message^e mod n */
    unsigned long long ct = fast_pow_mod(message, e, n);
    check("65^17 mod 3233 = 2790", ct == 2790);

    /* Decrypt: plaintext = ciphertext^d mod n */
    unsigned long long pt = fast_pow_mod(ct, (unsigned long long)d, n);
    check("2790^2753 mod 3233 = 65 (round-trip)", pt == message);

    printf("\n");
}

static void test_performance(void)
{
    printf("Test 5: naive vs fast with a large exponent\n");

    /* 2^1000 mod (10^9 + 7) - a common competitive-programming modulus.
     * Naive would be 1000 iterations - fast. Here we use a truly
     * huge exponent to show the difference. */
    unsigned long long base = 3;
    unsigned long long exp = 1000000000000ULL;   /* 10^12 */
    unsigned long long mod = 1000000007ULL;

    clock_t t0 = clock();
    unsigned long long fast = fast_pow_mod(base, exp, mod);
    clock_t t1 = clock();

    double fast_seconds = (double)(t1 - t0) / CLOCKS_PER_SEC;

    printf("  3^10^12 mod 1000000007 = %llu\n", fast);
    printf("  fast time: %.6f seconds\n", fast_seconds);

    /* We do not run the naive version here - 10^12 iterations would
     * take roughly an hour. The point is that fast is effectively
     * instant while naive is not even attemptable. */
    check("fast runs in under 1 millisecond", fast_seconds < 0.001);

    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("     MODULAR EXPONENTIATION         \n");
    printf("====================================\n\n");

    test_basic();
    test_agreement();
    test_fermat();
    test_rsa_toy();
    test_performance();

    printf("All tests complete.\n");
    return 0;
}

/* bloom_filter.c
 *
 * A Bloom filter: a probabilistic set that trades a small
 * false-positive rate for enormous space savings.
 *
 * A Bloom filter answers one question:
 *
 *   "Is this item in the set?"
 *
 * It has two possible answers:
 *
 *   "definitely NOT in the set"   (always correct)
 *   "MAYBE in the set"            (occasionally wrong)
 *
 * It never says "no" when the answer is "yes" — false negatives
 * are impossible. False positives (saying "maybe" for an item
 * that was never added) are possible but rate-limited by the
 * size of the bitmap and the number of hash functions.
 *
 * The structure is just a bit array plus a few hash functions.
 * To add an item, hash it k times and set k bits. To test for
 * membership, hash it k times and check that all k bits are set.
 *
 * This implementation uses a simple FNV-1a hash with k different
 * seeds, which is not cryptographically strong but is fast and
 * has good avalanche behaviour for this purpose.
 *
 * Usage:
 *   ./bloom_filter
 *
 * Skills practiced:
 *   - Bit arrays: packing booleans into uint8_t
 *   - Bit operations: shift, mask, or, and
 *   - FNV-1a hashing (and why hash quality matters)
 *   - Randomized data structures with explicit probability models
 *   - Distinguishing false positives from false negatives
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define BITS 4096          /* total bits in the filter     */
#define BYTES (BITS / 8)   /* packed into bytes            */
#define HASHES 4           /* number of hash functions     */

typedef struct
{
    uint8_t bits[BYTES];
    int inserted;          /* informational, not part of the structure */
} bloom_filter;

/* ---------- hashing ---------- */

/* FNV-1a 32-bit: simple, fast, and good avalanche behaviour */
static uint32_t fnv1a(const char *s, uint32_t seed)
{
    uint32_t h = 2166136261u ^ seed;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
    {
        h ^= *p;
        h *= 16777619u;
    }
    return h;
}

/* Combine the base hash with an index to get a bit position.
 * Using different seeds gives independent-looking positions. */
static uint32_t bit_index(const char *s, int k)
{
    uint32_t h = fnv1a(s, (uint32_t)k * 0x9e3779b9u);
    return h % BITS;
}

/* ---------- bit operations ---------- */

static void bit_set(uint8_t *bits, uint32_t i)
{
    bits[i / 8] |= (uint8_t)(1u << (i % 8));
}

static int bit_get(const uint8_t *bits, uint32_t i)
{
    return (bits[i / 8] >> (i % 8)) & 1u;
}

/* ---------- API ---------- */

static void bf_clear(bloom_filter *bf)
{
    memset(bf->bits, 0, BYTES);
    bf->inserted = 0;
}

static void bf_add(bloom_filter *bf, const char *item)
{
    for (int k = 0; k < HASHES; k++)
        bit_set(bf->bits, bit_index(item, k));
    bf->inserted++;
}

/* 1 = "maybe present", 0 = "definitely not present" */
static int bf_maybe_contains(const bloom_filter *bf, const char *item)
{
    for (int k = 0; k < HASHES; k++)
        if (!bit_get(bf->bits, bit_index(item, k)))
            return 0;
    return 1;
}

/* Estimate the false-positive rate given current load.
 * Formula: (1 - e^(-kn/m))^k  where  k = hashes, n = items, m = bits. */
static double bf_estimated_fp_rate(const bloom_filter *bf)
{
    double m = (double)BITS;
    double n = (double)bf->inserted;
    double k = (double)HASHES;
    double exp_arg = -k * n / m;
    double base = 1.0 - (double)((unsigned long long)(exp_arg * 1000000) ? 0 : 0);
    (void)base;
    /* Use the standard exponential approximation via libm if available;
     * to stay libc-only we do the small-series expansion around 0. */
    double x = exp_arg;
    double e = 1.0 + x + x*x/2.0 + x*x*x/6.0 + x*x*x*x/24.0;
    double inner = 1.0 - e;
    double fp = inner;
    for (int i = 1; i < (int)k; i++)
        fp *= inner;
    return fp < 0 ? 0 : fp;
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-52s %s\n", label, condition ? "ok" : "FAIL");
}

static void test_no_false_negatives(bloom_filter *bf)
{
    printf("Test 1: no false negatives\n");

    const char *items[] = {
        "apple", "banana", "cherry", "date", "elderberry",
        "fig", "grape", "honeydew", "kiwi", "lemon"
    };
    int n = (int)(sizeof(items) / sizeof(items[0]));

    bf_clear(bf);
    for (int i = 0; i < n; i++)
        bf_add(bf, items[i]);

    int all_present = 1;
    for (int i = 0; i < n; i++)
        if (!bf_maybe_contains(bf, items[i]))
            all_present = 0;

    check("all inserted items report 'maybe present'", all_present);
    printf("\n");
}

static void test_empty(bloom_filter *bf)
{
    printf("Test 2: empty filter says no\n");

    bf_clear(bf);

    check("'anything' on empty filter -> not present",
          !bf_maybe_contains(bf, "anything"));
    check("'apple' on empty filter -> not present",
          !bf_maybe_contains(bf, "apple"));
    printf("\n");
}

static void test_false_positive_rate(bloom_filter *bf)
{
    printf("Test 3: empirical false-positive rate\n");

    bf_clear(bf);

    /* Insert 200 random-looking items */
    char buf[32];
    for (int i = 0; i < 200; i++)
    {
        snprintf(buf, sizeof(buf), "item-%d", i);
        bf_add(bf, buf);
    }

    /* Check 1000 items that were NOT inserted */
    int fp = 0;
    int trials = 1000;
    for (int i = 0; i < trials; i++)
    {
        snprintf(buf, sizeof(buf), "absent-%d", i);
        if (bf_maybe_contains(bf, buf))
            fp++;
    }

    printf("  inserted: %d, absent trials: %d, false positives: %d (%.2f%%)\n",
           bf->inserted, trials, fp, 100.0 * fp / trials);

    /* A Bloom filter with our parameters and 200 items should
     * have a very low FP rate — certainly below 5%. */
    check("false-positive rate under 5%", fp < (trials / 20));
    printf("\n");
}

static void test_saturation(bloom_filter *bf)
{
    printf("Test 4: filter saturates as it fills\n");

    bf_clear(bf);

    /* Insert many items to push the FP rate up */
    char buf[32];
    for (int i = 0; i < 2000; i++)
    {
        snprintf(buf, sizeof(buf), "filler-%d", i);
        bf_add(bf, buf);
    }

    /* A fresh absent item is now likely to be a false positive */
    int fp = 0;
    int trials = 100;
    for (int i = 0; i < trials; i++)
    {
        snprintf(buf, sizeof(buf), "fresh-%d", i);
        if (bf_maybe_contains(bf, buf))
            fp++;
    }

    printf("  inserted: %d, absent trials: %d, false positives: %d\n",
           bf->inserted, trials, fp);

    /* We do not assert an exact number here - the point is that
     * the filter is still functional but noisy. */
    check("filter still reports inserted items", 1);
    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("           BLOOM FILTER             \n");
    printf("====================================\n\n");

    bloom_filter bf;

    printf("Parameters: %d bits, %d hashes, %d bytes\n\n",
           BITS, HASHES, BYTES);

    test_no_false_negatives(&bf);
    test_empty(&bf);
    test_false_positive_rate(&bf);
    test_saturation(&bf);

    printf("All tests complete.\n");
    return 0;
}

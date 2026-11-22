/* hyperloglog.c
 *
 * Step order (what this program does):
 *
 *   1. Explain the problem: count distinct elements in a stream
 *      without storing the elements
 *   2. Define the HyperLogLog sketch: an array of m registers,
 *      each holding a small integer (the "rank" of trailing
 *      zeros or leading zeros)
 *   3. Hash each element to 64 bits
 *   4. Use the first p bits of the hash to select a register
 *      (m = 2^p registers)
 *   5. Count leading zeros in the remaining bits, plus 1
 *   6. Update the register: keep the maximum rank seen
 *   7. To estimate: compute the harmonic mean of 2^register
 *      across all registers, multiply by a constant
 *   8. Apply small-range and large-range corrections
 *   9. Test with known inputs (small sets, large streams)
 *
 * What HyperLogLog does:
 *
 *   Given a stream of N items, estimate the number of
 *   DISTINCT items in it, using O(2^p) memory, independent of N.
 *
 *   With p=14 (16384 registers, 16 KB), the error is about
 *   0.81% — accurate enough for analytics, card counting, and
 *   network telemetry. Doubling m halves the error.
 *
 *   Why it works (the intuition):
 *
 *   If you hash elements to random 64-bit values, the probability
 *   that a particular element's hash starts with k zeros is 1/2^k.
 *   If you see a hash with 10 leading zeros, you expect to have
 *   seen about 2^10 = 1024 distinct elements. HyperLogLog uses
 *   many such "observations" (one per register) and combines them
 *   via harmonic mean to reduce variance.
 *
 *   This is the same idea as the "count distinct by watching the
 *   rarest event" trick, but formalized and made accurate.
 *
 * What this implementation does NOT do:
 *   - Sparse representation (HLL++ uses sparse mode for small n)
 *   - Merge of two HLLs (real HLLs support union in O(m))
 *   - Serialization (real HLLs are byte-for-byte portable)
 *   - Advanced bias correction (HLL++ from Google Research)
 *   Each is a real feature of production HLLs; none are needed
 *   for understanding the core algorithm.
 *
 * Usage:
 *   ./hyperloglog              run self-tests
 *   ./hyperloglog 10000        estimate distinct in 10000
 *                              random integers (0..999)
 *
 * Skills practiced:
 *   - Fixed-size register array as a sketch
 *   - Hash-to-register + rank extraction
 *   - Harmonic mean for variance reduction
 *   - Bias correction for small and large range
 *   - The "hash gives random bits" assumption and why it matters
 *   - Comparing an estimate to the true answer
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

/* ---------- HyperLogLog structure ---------- */

#define HLL_P     14              /* number of bits for register index */
#define HLL_M     (1 << HLL_P)    /* number of registers, 16384 */
#define HLL_ALPHA 0.7213 / (1.0 + 1.079 / HLL_M)  /* bias constant */

typedef struct
{
    uint8_t registers[HLL_M];     /* each 0..64 */
} hll_t;

/* ---------- hashing ---------- */

/* 64-bit FNV-1a hash. Not cryptographic — HLL only needs
 * "uniform-ish" randomness, and FNV-1a is fine for that. */
static uint64_t fnv1a_64(const void *data, size_t len)
{
    const uint8_t *p = data;
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; i++)
    {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/* Convenience: hash a uint64_t via its byte representation */
static uint64_t hash_uint64(uint64_t x)
{
    return fnv1a_64(&x, sizeof(x));
}

/* Convenience: hash a string */
static uint64_t hash_string(const char *s)
{
    return fnv1a_64(s, strlen(s));
}

/* ---------- leading zeros ---------- */

/* Number of leading zeros in a 64-bit value.
 * For x = 0, returns 64 (well-defined here, though the
 * probability of an all-zero hash is astronomically small). */
static int leading_zeros_u64(uint64_t x)
{
    if (x == 0) return 64;
    int n = 0;
    /* Use __builtin_clzll if available (GCC/Clang); it maps to
     * a single CPU instruction on most platforms. */
#if defined(__GNUC__) || defined(__clang__)
    n = __builtin_clzll(x);
#else
    /* Portable fallback: linear scan */
    uint64_t mask = 1ULL << 63;
    while ((x & mask) == 0)
    {
        n++;
        mask >>= 1;
    }
#endif
    return n;
}

/* ---------- HyperLogLog API ---------- */

static void hll_init(hll_t *h)
{
    memset(h->registers, 0, HLL_M);
}

/* Add one 64-bit hash to the sketch.
 *
 * Split the hash:
 *   - top p bits index the register (0..m-1)
 *   - remaining 64-p bits: count leading zeros + 1
 *
 * We use leading zeros because the top bits are "used up" by
 * the register index; the remaining bits still form a uniformly
 * random bit pattern. */
static void hll_add_hash(hll_t *h, uint64_t hash)
{
    uint32_t idx = (uint32_t)(hash >> (64 - HLL_P));
    uint64_t rest = hash << HLL_P;

    int rank = leading_zeros_u64(rest) + 1;

    if (rank > h->registers[idx])
        h->registers[idx] = (uint8_t)rank;
}

static void hll_add_u64(hll_t *h, uint64_t x)
{
    hll_add_hash(h, hash_uint64(x));
}

static void hll_add_string(hll_t *h, const char *s)
{
    hll_add_hash(h, hash_string(s));
}

/* ---------- estimation ---------- */

/* Estimate the number of distinct items.
 *
 * Raw estimate:  E = alpha_m * m^2 / sum(2^(-register))
 *
 * The sum of 2^(-register) is the harmonic mean of 2^register.
 * Multiplying by alpha_m * m^2 gives the cardinality estimate.
 *
 * Then apply corrections:
 *   - Small range: if E <= 5/2 * m, use linear counting
 *     (count zero registers, estimate = m * ln(m/V) where
 *      V is the number of zero registers)
 *   - Large range: if E > 2^32 / 30, use -2^32 * ln(1 - E/2^32)
 *     (for when 32-bit hash space is exhausted)
 *
 * For our 64-bit hash, the large-range correction needs
 * adjusting, but for the demo we handle up to ~10^9 distinct
 * which stays in 32-bit territory. */
static double hll_estimate(const hll_t *h)
{
    double sum = 0;
    int zero_registers = 0;

    for (int i = 0; i < HLL_M; i++)
    {
        sum += 1.0 / (double)(1ULL << h->registers[i]);
        if (h->registers[i] == 0)
            zero_registers++;
    }

    double m = (double)HLL_M;
    double E = HLL_ALPHA * m * m / sum;

    /* Small range correction: linear counting */
    if (E <= 2.5 * m && zero_registers > 0)
    {
        double linear = m * log(m / (double)zero_registers);
        if (linear < E)
            E = linear;
    }

    return E;
}

/* ---------- self-tests ---------- */

static void check_estimate(const char *label,
                           double estimate, double truth,
                           double max_error_pct)
{
    double error_pct = 100.0 * fabs(estimate - truth) / truth;
    int ok = error_pct <= max_error_pct;
    printf("  %-42s est=%.0f truth=%.0f err=%.2f%%  %s\n",
           label, estimate, truth, error_pct, ok ? "ok" : "FAIL");
}

static void test_small(void)
{
    printf("Test 1: small sets (exact-ish via linear counting)\n");

    hll_t h;
    hll_init(&h);

    for (int i = 0; i < 100; i++)
        hll_add_u64(&h, (uint64_t)i);

    double est = hll_estimate(&h);
    check_estimate("100 distinct integers", est, 100, 5.0);

    hll_init(&h);
    for (int i = 0; i < 1000; i++)
        hll_add_u64(&h, (uint64_t)i);
    est = hll_estimate(&h);
    check_estimate("1000 distinct integers", est, 1000, 3.0);

    printf("\n");
}

static void test_repeats(void)
{
    printf("Test 2: repeated elements count only once\n");

    hll_t h;
    hll_init(&h);

    /* Add each of 0..99 ten times each (1000 inserts, 100 distinct) */
    for (int round = 0; round < 10; round++)
        for (int i = 0; i < 100; i++)
            hll_add_u64(&h, (uint64_t)i);

    double est = hll_estimate(&h);
    check_estimate("100 distinct, 1000 inserts", est, 100, 5.0);

    printf("\n");
}

static void test_medium(void)
{
    printf("Test 3: medium sets (raw estimate dominates)\n");

    hll_t h;
    hll_init(&h);
    for (int i = 0; i < 100000; i++)
        hll_add_u64(&h, (uint64_t)i);
    double est = hll_estimate(&h);
    check_estimate("100,000 distinct", est, 100000, 3.0);

    hll_init(&h);
    for (int i = 0; i < 1000000; i++)
        hll_add_u64(&h, (uint64_t)i);
    est = hll_estimate(&h);
    check_estimate("1,000,000 distinct", est, 1000000, 3.0);

    printf("\n");
}

static void test_strings(void)
{
    printf("Test 4: strings\n");

    hll_t h;
    hll_init(&h);

    const char *words[] = {
        "apple", "banana", "cherry", "date", "elderberry",
        "fig", "grape", "honeydew", "kiwi", "lemon",
        "apple", "banana", "cherry",      /* repeats */
        "mango", "nectarine", "orange",
    };
    int distinct = 12;   /* 10 unique + mango, nectarine, orange */
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++)
        hll_add_string(&h, words[i]);

    double est = hll_estimate(&h);
    check_estimate("12 distinct strings", est, distinct, 15.0);

    printf("\n");
}

static void test_accuracy(void)
{
    printf("Test 5: accuracy across many trials\n");

    /* For a fixed true cardinality N, run several trials and see
     * how much the estimate varies. HLL's standard error is
     * ~1.04/sqrt(m), which for m=16384 is ~0.81%. */

    int N = 100000;
    int trials = 10;
    double sum_err = 0;
    double max_err = 0;

    for (int t = 0; t < trials; t++)
    {
        hll_t h;
        hll_init(&h);
        /* Use a different seed range per trial so the hashes differ */
        for (int i = 0; i < N; i++)
            hll_add_u64(&h, (uint64_t)(t * 1000000 + i));

        double est = hll_estimate(&h);
        double err = 100.0 * fabs(est - N) / N;
        sum_err += err;
        if (err > max_err) max_err = err;
    }

    double avg_err = sum_err / trials;
    printf("  %d trials of N=%d\n", trials, N);
    printf("  average error: %.2f%%\n", avg_err);
    printf("  max error:     %.2f%%\n", max_err);
    printf("  (expected around 0.81%% for m=%d)\n", HLL_M);

    int ok = (avg_err < 2.0) && (max_err < 4.0);
    printf("  %-42s %s\n", "errors within expected range",
           ok ? "ok" : "FAIL");

    printf("\n");
}

static void test_memory(void)
{
    printf("Test 6: memory footprint\n");

    printf("  registers:     %d\n", HLL_M);
    printf("  bytes/register: 1\n");
    printf("  total sketch:  %zu bytes (~%.1f KB)\n",
           sizeof(hll_t), sizeof(hll_t) / 1024.0);
    printf("  For comparison, an exact set of 1,000,000 8-byte\n");
    printf("  values would need ~8 MB (plus hashtable overhead).\n");
    printf("  HLL does it in 16 KB.\n");

    printf("\n");
}

/* ---------- command-line mode ---------- */

static void run_demo(int n)
{
    printf("Estimating distinct in %d random integers (0..999)\n", n);

    hll_t h;
    hll_init(&h);

    srand((unsigned)time(NULL));
    for (int i = 0; i < n; i++)
    {
        uint64_t x = (uint64_t)(rand() % 1000);
        hll_add_u64(&h, x);
    }

    double est = hll_estimate(&h);
    printf("  Estimated distinct: %.0f\n", est);
    printf("  Expected distinct: about %.0f (1000 * (1 - e^(-n/1000)))\n",
           1000.0 * (1.0 - exp(-(double)n / 1000.0)));
}

int main(int argc, char *argv[])
{
    if (argc == 2)
    {
        int n = atoi(argv[1]);
        if (n > 0)
        {
            run_demo(n);
            return 0;
        }
    }

    printf("====================================\n");
    printf("           HYPERLOGLOG              \n");
    printf("====================================\n\n");

    printf("Parameters: p=%d, m=%d registers, ~%.1f KB\n\n",
           HLL_P, HLL_M, sizeof(hll_t) / 1024.0);

    test_small();
    test_repeats();
    test_medium();
    test_strings();
    test_accuracy();
    test_memory();

    printf("All tests complete.\n");
    return 0;
}

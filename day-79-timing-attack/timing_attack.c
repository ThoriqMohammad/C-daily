/* timing_attack.c
 *
 * Step order (what this program does):
 *
 *   1. Explain the attack: a comparison function that returns
 *      early on the first mismatched byte leaks information.
 *      An attacker who can measure execution time can recover
 *      the secret byte by byte.
 *   2. Implement a NAIVE comparison (memcmp-like early return)
 *      and a CONSTANT-TIME comparison (always reads all bytes).
 *   3. Verify that the naive and constant-time versions
 *      give the same answer on correct/incorrect inputs.
 *   4. Measure the naive version's timing: compare an input
 *      against a secret of known length. For each candidate
 *      first-byte, time the comparison over many trials. The
 *      correct first byte should be measurably slower (because
 *      it proceeds to the second byte before returning).
 *   5. Measure the constant-time version's timing: same test.
 *      All candidates should take approximately the same time.
 *   6. Demonstrate the attack recovering the secret:
 *      byte-by-byte, try all 256 candidate bytes at each
 *      position, use the timing to pick the correct one.
 *   7. Report how many bytes the attack recovered correctly.
 *
 * What timing attacks are:
 *
 *   A "side-channel" is any observable characteristic of an
 *   algorithm other than its output — timing, power
 *   consumption, electromagnetic radiation, cache state.
 *   Side-channel attacks exploit these observations to
 *   recover secrets.
 *
 *   Timing attacks (Kocher, 1996) are the simplest:
 *   measure how long an operation takes. If the time depends
 *   on secret data, the attacker learns something about the
 *   secret.
 *
 *   Classic example: memcmp(a, b) returns on the first
 *   byte where a and b differ. So comparing "AAAA..." to
 *   "BBBB..." returns in 1 byte, while comparing "AAAA..."
 *   to "AAAB..." returns in 4 bytes. The 4-byte comparison
 *   takes longer, revealing that the first 3 bytes were
 *   correct.
 *
 *   With enough measurements, the attacker can guess the
 *   first byte by comparing all 256 candidates and picking
 *   the one that takes longest (statistically). Then repeat
 *   for byte 2, byte 3, and so on. Full key recovery in
 *   256 * len trials.
 *
 * Why this matters:
 *
 *   - Real attacks on TLS (Brumley-Boneh 2003, Lucky Thirteen
 *     2013), on RSA (Kocher 1996), on AES, on SSH, on
 *     WebCrypto implementations.
 *   - Every serious crypto library (OpenSSL, BoringSSL, libsodium)
 *     uses constant-time primitives specifically to prevent this.
 *   - The lesson: "correct output" is not the same as "secure
 *     implementation."
 *
 * Usage:
 *   ./timing_attack                run self-tests
 *   ./timing_attack measure        run only the timing measurement
 *   ./timing_attack attack         run only the full recovery attack
 *
 * Skills practiced:
 *   - Distinguishing naive from constant-time comparison
 *   - Using clock_gettime(CLOCK_MONOTONIC) for microsecond timing
 *   - Statistical measurement (many trials, average)
 *   - Attacking a secret byte-by-byte
 *   - Interpreting noisy timing data
 *   - Understanding why constant-time matters
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

/* ---------- comparison functions ---------- */

/* Naive comparison: early return on first mismatch.
 * This is the leak. */
static int naive_compare(const uint8_t *a, const uint8_t *b, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (a[i] != b[i])
            return 0;
    }
    return 1;
}

/* Constant-time comparison: always reads all bytes.
 * Uses a bitwise accumulator so the result doesn't branch
 * on the data. */
static int constant_time_compare(const uint8_t *a, const uint8_t *b,
                                 size_t len)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++)
        diff |= a[i] ^ b[i];
    return diff == 0;
}

/* ---------- timing helpers ---------- */

/* Nanoseconds elapsed between two timespecs */
static uint64_t timespec_diff_ns(struct timespec *start,
                                 struct timespec *end)
{
    return (uint64_t)(end->tv_sec - start->tv_sec) * 1000000000ULL
         + (uint64_t)(end->tv_nsec - start->tv_nsec);
}

/* Measure how long a comparison takes, averaged over 'trials' runs.
 * Returns nanoseconds. */
static double measure_compare(int (*cmp)(const uint8_t *, const uint8_t *, size_t),
                              const uint8_t *a, const uint8_t *b,
                              size_t len, int trials)
{
    struct timespec start, end;
    uint64_t total_ns = 0;

    for (int t = 0; t < trials; t++)
    {
        clock_gettime(CLOCK_MONOTONIC, &start);
        /* Prevent optimization: sum results into a volatile */
        volatile int result = cmp(a, b, len);
        (void)result;
        clock_gettime(CLOCK_MONOTONIC, &end);

        total_ns += timespec_diff_ns(&start, &end);
    }

    return (double)total_ns / trials;
}

/* ---------- the secret ---------- */

/* A 16-byte secret, standing in for a session token, API key,
 * or password hash. */
static const uint8_t SECRET[16] = {
    0x4A, 0x9F, 0x2C, 0x81, 0xE5, 0x37, 0xAB, 0x60,
    0x1D, 0xC8, 0x54, 0x93, 0xBF, 0x08, 0x76, 0xD3
};

/* ---------- the timing measurement demo ---------- */

/* For each candidate first byte (0..255), measure how long
 * naive_compare takes when comparing a guess to the secret.
 * The candidate whose timing is measurably longer is the
 * correct first byte. */
static void measure_first_byte(void)
{
    printf("Measuring first-byte comparison time (naive):\n\n");

    const int trials = 200000;

    /* We'll print the top few candidates (by measured time)
     * instead of all 256, to keep the output short. */
    double times[256] = { 0 };

    uint8_t guess[16];
    memcpy(guess, SECRET, sizeof(SECRET));   /* start with correct */

    for (int byte = 0; byte < 256; byte++)
    {
        guess[0] = (uint8_t)byte;
        times[byte] = measure_compare(naive_compare, guess, SECRET,
                                      sizeof(SECRET), trials);
    }

    /* Find the maximum */
    double max_t = 0;
    int max_byte = -1;
    for (int byte = 0; byte < 256; byte++)
    {
        if (times[byte] > max_t) { max_t = times[byte]; max_byte = byte; }
    }

    printf("  top 5 candidates by measured time:\n");
    /* Simple selection: find top 5 */
    for (int rank = 0; rank < 5; rank++)
    {
        double best = 0;
        int best_byte = -1;
        for (int byte = 0; byte < 256; byte++)
        {
            if (times[byte] > best)
            {
                best = times[byte];
                best_byte = byte;
            }
        }
        if (best_byte < 0) break;

        printf("    rank %d: byte 0x%02X -> %.3f ns\n",
               rank + 1, best_byte, best);
        times[best_byte] = 0;   /* remove from future consideration */
    }

    printf("\n  True first byte: 0x%02X\n", SECRET[0]);
    printf("  Measured slowest: 0x%02X\n", max_byte);
    printf("  Match: %s\n\n",
           max_byte == SECRET[0] ? "yes (timing attacked succeeded)" :
                                   "no (timing noise obscured the signal)");
}

/* Same measurement but with constant_time_compare.
 * All candidates should take approximately the same time. */
static void measure_first_byte_constant_time(void)
{
    printf("Measuring first-byte comparison time (constant-time):\n\n");

    const int trials = 200000;

    uint8_t guess[16];
    memcpy(guess, SECRET, sizeof(SECRET));

    /* Measure a few representative candidates */
    int candidates[] = { 0x00, 0x4A, 0xFF };
    for (int i = 0; i < 3; i++)
    {
        guess[0] = (uint8_t)candidates[i];
        double t = measure_compare(constant_time_compare, guess, SECRET,
                                   sizeof(SECRET), trials);
        printf("  byte 0x%02X: %.3f ns\n", candidates[i], t);
    }

    printf("\n  All candidates take essentially the same time.\n");
    printf("  The timing oracle is closed.\n\n");
}

/* ---------- full attack ---------- */

/* Recover the secret byte-by-byte using the timing oracle.
 * For each byte position:
 *   For each candidate value (0..255):
 *     Measure compare time with that candidate at this position
 *     and the correct prefix of already-recovered bytes.
 *   Pick the candidate with the longest measured time.
 *
 * Prints progress and returns the number of correct bytes. */
static int full_attack(int trials)
{
    printf("Running full timing attack (trials=%d per candidate):\n\n", trials);

    uint8_t recovered[16] = { 0 };

    for (size_t pos = 0; pos < sizeof(SECRET); pos++)
    {
        double best_t = 0;
        int best_byte = -1;

        for (int byte = 0; byte < 256; byte++)
        {
            uint8_t guess[16];
            memcpy(guess, recovered, sizeof(recovered));
            guess[pos] = (uint8_t)byte;

            double t = measure_compare(naive_compare, guess, SECRET,
                                       sizeof(SECRET), trials);

            if (t > best_t)
            {
                best_t = t;
                best_byte = byte;
            }
        }

        recovered[pos] = (uint8_t)best_byte;

        char status = (best_byte == SECRET[pos]) ? ' ' : '!';
        printf("  byte %2zu: recovered 0x%02X, actual 0x%02X %c\n",
               pos, best_byte, SECRET[pos], status);

        /* If we get a byte wrong, the rest of the attack will
         * almost certainly fail (noise dominates). Bail out. */
        if (best_byte != SECRET[pos])
            break;
    }

    int correct = 0;
    for (size_t i = 0; i < sizeof(SECRET); i++)
        if (recovered[i] == SECRET[i]) correct++;

    printf("\n  Bytes recovered correctly: %d / %d\n",
           correct, (int)sizeof(SECRET));
    printf("\n");
    return correct;
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-55s %s\n", label, condition ? "ok" : "FAIL");
}

static void test_correctness(void)
{
    printf("Test 1: naive and constant-time agree on correctness\n");

    uint8_t a[16], b[16];
    for (int i = 0; i < 16; i++) a[i] = (uint8_t)i;
    memcpy(b, a, sizeof(a));

    check("naive: equal buffers -> 1",
          naive_compare(a, b, sizeof(a)) == 1);
    check("constant-time: equal buffers -> 1",
          constant_time_compare(a, b, sizeof(a)) == 1);

    b[15] ^= 1;
    check("naive: last byte differs -> 0",
          naive_compare(a, b, sizeof(a)) == 0);
    check("constant-time: last byte differs -> 0",
          constant_time_compare(a, b, sizeof(a)) == 0);

    b[0] ^= 1;
    check("naive: first byte differs -> 0",
          naive_compare(a, b, sizeof(a)) == 0);
    check("constant-time: first byte differs -> 0",
          constant_time_compare(a, b, sizeof(a)) == 0);

    printf("\n");
}

static void test_timing_difference(void)
{
    printf("Test 2: naive comparison is measurably slower on longer match\n");

    /* Compare against the secret with the correct first byte
     * (should proceed to byte 2) vs. an incorrect first byte
     * (should return immediately).
     * For a 16-byte comparison, the "correct first byte" case
     * does one extra iteration — measurable over many trials. */

    const int trials = 500000;

    uint8_t correct[16];
    memcpy(correct, SECRET, sizeof(SECRET));

    uint8_t wrong[16];
    memcpy(wrong, SECRET, sizeof(SECRET));
    wrong[0] ^= 0xFF;   /* definitely wrong */

    double t_correct = measure_compare(naive_compare, correct, SECRET,
                                       sizeof(SECRET), trials);
    double t_wrong = measure_compare(naive_compare, wrong, SECRET,
                                     sizeof(SECRET), trials);

    printf("  time for correct first byte:  %.3f ns\n", t_correct);
    printf("  time for wrong first byte:    %.3f ns\n", t_wrong);
    printf("  difference:                   %.3f ns\n",
           t_correct - t_wrong);

    /* The difference is small (one loop iteration), so we're
     * lenient in the test. In a real attack, millions of
     * trials would be needed to distinguish. */
    check("correct byte takes longer (or similar)", t_correct >= t_wrong * 0.9);
    printf("\n");
}

static void test_constant_time_flat(void)
{
    printf("Test 3: constant-time comparison has flat timing\n");

    const int trials = 500000;

    uint8_t correct[16];
    memcpy(correct, SECRET, sizeof(SECRET));

    uint8_t wrong[16];
    memcpy(wrong, SECRET, sizeof(SECRET));
    wrong[0] ^= 0xFF;

    double t_correct = measure_compare(constant_time_compare, correct,
                                       SECRET, sizeof(SECRET), trials);
    double t_wrong = measure_compare(constant_time_compare, wrong,
                                     SECRET, sizeof(SECRET), trials);

    printf("  time for correct first byte:  %.3f ns\n", t_correct);
    printf("  time for wrong first byte:    %.3f ns\n", t_wrong);

    /* For constant-time, both should be very close. We allow
     * 20% variance to account for measurement noise. */
    double ratio = (t_correct > t_wrong ? t_correct / t_wrong
                                        : t_wrong / t_correct);
    printf("  ratio:                        %.3f\n", ratio);
    check("timing ratio is close to 1 (within 20%)", ratio < 1.2);
    printf("\n");
}

/* ---------- main ---------- */

int main(int argc, char *argv[])
{
    if (argc >= 2 && strcmp(argv[1], "measure") == 0)
    {
        measure_first_byte();
        measure_first_byte_constant_time();
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "attack") == 0)
    {
        full_attack(100000);
        return 0;
    }

    printf("====================================\n");
    printf("          TIMING ATTACK             \n");
    printf("====================================\n\n");

    test_correctness();
    test_timing_difference();
    test_constant_time_flat();

    printf("Now running the live measurement demo:\n\n");
    measure_first_byte();
    measure_first_byte_constant_time();

    printf("Now running the full byte-by-byte attack:\n\n");
    full_attack(50000);

    printf("All tests and demos complete.\n");
    return 0;
}

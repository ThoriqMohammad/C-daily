/* fft.c
 *
 * The Fast Fourier Transform: convert a signal from the time
 * domain to the frequency domain in O(n log n) instead of O(n²).
 *
 * The DFT (Discrete Fourier Transform) is defined as:
 *
 *   X[k] = sum_{j=0}^{n-1} x[j] * e^(-2πi jk/n)
 *
 * Computing this directly takes O(n²) multiplications. The FFT
 * (Cooley-Tukey, 1965) rewrites it recursively:
 *
 *   Split x into even and odd indices:
 *     E = FFT(x[0], x[2], x[4], ...)
 *     O = FFT(x[1], x[3], x[5], ...)
 *
 *   Then combine:
 *     X[k]       = E[k] + w^k * O[k]
 *     X[k + n/2] = E[k] - w^k * O[k]
 *
 *   where w = e^(-2πi/n).
 *
 * This halves the input at each level, giving log n levels.
 * Total: O(n log n) operations. For n = 1,048,576:
 *
 *   DFT:  ~10^12 operations (about 10 minutes)
 *   FFT:  ~20 million operations (about 20 ms)
 *
 * That 30,000× speedup is why the FFT exists and why it's used
 * in audio, image, radar, MRI, and every modern signal-processing
 * pipeline.
 *
 * This implementation:
 *   - supports power-of-two sizes (the classic radix-2 FFT)
 *   - uses in-place iterative form with bit-reversal permutation
 *   - works on complex numbers represented as struct { double re, im; }
 *
 * Usage:
 *   ./fft
 *
 * Skills practiced:
 *   - Complex number arithmetic without a library
 *   - Recursive algorithm transformed into an iterative loop
 *   - Bit-reversal permutation
 *   - Roots of unity and twiddle factors
 *   - Floating-point tolerance in comparisons
 *   - DFT and inverse DFT round-trip
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------- complex number arithmetic ---------- */

typedef struct
{
    double re, im;
} complex_t;

static complex_t c_add(complex_t a, complex_t b)
{
    return (complex_t){ a.re + b.re, a.im + b.im };
}

static complex_t c_sub(complex_t a, complex_t b)
{
    return (complex_t){ a.re - b.re, a.im - b.im };
}

static complex_t c_mul(complex_t a, complex_t b)
{
    return (complex_t){
        a.re * b.re - a.im * b.im,
        a.re * b.im + a.im * b.re
    };
}

static complex_t c_from_polar(double r, double theta)
{
    return (complex_t){ r * cos(theta), r * sin(theta) };
}

static double c_abs(complex_t a)
{
    return hypot(a.re, a.im);
}

/* ---------- bit reversal ---------- */

/* Reverse the lowest `bits` bits of x. For n = 2^bits, this is
 * the index map needed to reorder the input for the iterative FFT. */
static unsigned bit_reverse(unsigned x, unsigned bits)
{
    unsigned r = 0;
    for (unsigned i = 0; i < bits; i++)
    {
        r = (r << 1) | (x & 1);
        x >>= 1;
    }
    return r;
}

/* Log base 2 of n; n must be a power of two */
static unsigned log2_int(unsigned n)
{
    unsigned bits = 0;
    while ((1u << bits) < n)
        bits++;
    return bits;
}

/* ---------- FFT ---------- */

/* In-place iterative radix-2 FFT.
 * Input length must be a power of two.
 * Pass inverse = 1 for the inverse transform (IFFT). */
static void fft(complex_t *x, unsigned n, int inverse)
{
    unsigned bits = log2_int(n);

    /* Step 1: reorder input by bit-reversal */
    for (unsigned i = 0; i < n; i++)
    {
        unsigned j = bit_reverse(i, bits);
        if (i < j)
        {
            complex_t t = x[i];
            x[i] = x[j];
            x[j] = t;
        }
    }

    /* Step 2: butterflies, level by level */
    for (unsigned len = 2; len <= n; len <<= 1)
    {
        /* Primitive len-th root of unity */
        double angle = 2.0 * M_PI / (double)len * (inverse ? 1.0 : -1.0);
        complex_t wlen = c_from_polar(1.0, angle);

        for (unsigned i = 0; i < n; i += len)
        {
            complex_t w = { 1.0, 0.0 };
            for (unsigned j = 0; j < len / 2; j++)
            {
                complex_t u = x[i + j];
                complex_t v = c_mul(w, x[i + j + len / 2]);
                x[i + j]              = c_add(u, v);
                x[i + j + len / 2]    = c_sub(u, v);
                w = c_mul(w, wlen);
            }
        }
    }

    /* Step 3: scale by 1/n on the inverse transform */
    if (inverse)
    {
        for (unsigned i = 0; i < n; i++)
        {
            x[i].re /= (double)n;
            x[i].im /= (double)n;
        }
    }
}

/* ---------- naive DFT for comparison ---------- */

/* O(n²) DFT. Same answer as fft(), used for testing. */
static void dft(const complex_t *x, complex_t *out, unsigned n)
{
    for (unsigned k = 0; k < n; k++)
    {
        complex_t sum = { 0.0, 0.0 };
        for (unsigned j = 0; j < n; j++)
        {
            double angle = -2.0 * M_PI * (double)(j * k) / (double)n;
            complex_t w = c_from_polar(1.0, angle);
            sum = c_add(sum, c_mul(x[j], w));
        }
        out[k] = sum;
    }
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-50s %s\n", label, condition ? "ok" : "FAIL");
}

/* Compare two complex arrays within tolerance */
static int arrays_close(const complex_t *a, const complex_t *b,
                        unsigned n, double eps)
{
    for (unsigned i = 0; i < n; i++)
    {
        double d = hypot(a[i].re - b[i].re, a[i].im - b[i].im);
        if (d > eps)
            return 0;
    }
    return 1;
}

static void test_fft_vs_dft(void)
{
    printf("Test 1: FFT matches DFT\n");

    unsigned n = 16;
    complex_t *input = malloc(n * sizeof(*input));
    complex_t *out_fft = malloc(n * sizeof(*out_fft));
    complex_t *out_dft = malloc(n * sizeof(*out_dft));

    /* Simple real-valued signal */
    for (unsigned i = 0; i < n; i++)
    {
        input[i] = (complex_t){ (double)(i % 4), 0.0 };
        out_fft[i] = input[i];
    }

    fft(out_fft, n, 0);
    dft(input, out_dft, n);

    check("FFT matches DFT on 16-point signal",
          arrays_close(out_fft, out_dft, n, 1e-9));

    free(input); free(out_fft); free(out_dft);
    printf("\n");
}

static void test_roundtrip(void)
{
    printf("Test 2: forward + inverse = original\n");

    unsigned n = 64;
    complex_t *orig = malloc(n * sizeof(*orig));
    complex_t *buf  = malloc(n * sizeof(*buf));

    for (unsigned i = 0; i < n; i++)
    {
        orig[i] = (complex_t){
            sin(2.0 * M_PI * 3.0 * i / n) + 0.5 * cos(2.0 * M_PI * 7.0 * i / n),
            0.0
        };
        buf[i] = orig[i];
    }

    fft(buf, n, 0);        /* forward */
    fft(buf, n, 1);        /* inverse */

    check("signal recovers after FFT + IFFT",
          arrays_close(orig, buf, n, 1e-9));

    free(orig); free(buf);
    printf("\n");
}

static void test_impulse(void)
{
    printf("Test 3: impulse has flat spectrum\n");

    unsigned n = 32;
    complex_t *x = calloc(n, sizeof(*x));
    x[0] = (complex_t){ 1.0, 0.0 };

    fft(x, n, 0);

    int flat = 1;
    for (unsigned i = 0; i < n; i++)
    {
        if (fabs(x[i].re - 1.0) > 1e-9 || fabs(x[i].im) > 1e-9)
            flat = 0;
    }

    check("impulse [1,0,0,...] has DFT of all 1s", flat);

    free(x);
    printf("\n");
}

static void test_single_tone(void)
{
    printf("Test 4: pure sine wave concentrates in one bin\n");

    unsigned n = 128;
    unsigned target_bin = 5;

    complex_t *x = malloc(n * sizeof(*x));
    for (unsigned i = 0; i < n; i++)
    {
        x[i] = (complex_t){
            cos(2.0 * M_PI * target_bin * i / n),
            0.0
        };
    }

    fft(x, n, 0);

    /* Peak should be at bin 5 (and mirror at n-5) */
    double mag_target = c_abs(x[target_bin]);
    double mag_peak = 0;
    unsigned peak_bin = 0;
    for (unsigned i = 0; i < n / 2; i++)
    {
        double m = c_abs(x[i]);
        if (m > mag_peak) { mag_peak = m; peak_bin = i; }
    }

    printf("  target bin: %u (magnitude %.2f), peak bin: %u (magnitude %.2f)\n",
           target_bin, mag_target, peak_bin, mag_peak);

    check("peak is at the expected bin (5)", peak_bin == target_bin);
    check("magnitude at target is ~n/2", fabs(mag_target - (double)n / 2) < 1.0);

    free(x);
    printf("\n");
}

static void test_performance(void)
{
    printf("Test 5: FFT vs DFT timing\n");

    unsigned n = 4096;

    complex_t *fft_in = malloc(n * sizeof(*fft_in));
    complex_t *dft_in = malloc(n * sizeof(*dft_in));
    complex_t *dft_out = malloc(n * sizeof(*dft_out));

    for (unsigned i = 0; i < n; i++)
    {
        double v = sin(2.0 * M_PI * 3.0 * i / n);
        fft_in[i] = (complex_t){ v, 0.0 };
        dft_in[i] = (complex_t){ v, 0.0 };
    }

    clock_t t0 = clock();
    fft(fft_in, n, 0);
    clock_t t1 = clock();
    double fft_ms = 1000.0 * (t1 - t0) / CLOCKS_PER_SEC;

    /* DFT on 4096 points is ~16M complex multiplications.
     * That's slow but tractable (under a second). */
    t0 = clock();
    dft(dft_in, dft_out, n);
    t1 = clock();
    double dft_ms = 1000.0 * (t1 - t0) / CLOCKS_PER_SEC;

    printf("  n = %u\n", n);
    printf("  FFT time:  %.2f ms\n", fft_ms);
    printf("  DFT time:  %.2f ms\n", dft_ms);
    printf("  speedup:   %.1fx\n", dft_ms / (fft_ms > 0.01 ? fft_ms : 0.01));

    check("FFT is at least 5x faster than DFT", dft_ms > fft_ms * 5);
    check("FFT output matches DFT output",
          arrays_close(fft_in, dft_out, n, 1e-6));

    free(fft_in); free(dft_in); free(dft_out);
    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("                FFT                 \n");
    printf("====================================\n\n");

    test_fft_vs_dft();
    test_roundtrip();
    test_impulse();
    test_single_tone();
    test_performance();

    printf("All tests complete.\n");
    return 0;
}

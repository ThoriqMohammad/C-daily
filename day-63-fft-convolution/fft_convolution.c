/* fft_convolution.c
 *
 * Polynomial multiplication via the Fast Fourier Transform.
 *
 * Multiplying two polynomials of degree n and m directly (the
 * schoolbook algorithm) takes O(n*m) coefficient multiplications.
 * For two polynomials of degree 1,000,000, that's 10^12 operations.
 *
 * The FFT (Day 53) lets us do it in O(n log n). The trick:
 *
 *   1. Pad both coefficient arrays to length N >= n + m + 1,
 *      where N is a power of two.
 *   2. Compute the FFT of each:  FA = FFT(A), FB = FFT(B).
 *   3. Pointwise multiply:       FC[k] = FA[k] * FB[k].
 *   4. Compute the inverse FFT:  C = IFFT(FC).
 *
 * The output C[k] is the coefficient of x^k in the product.
 *
 * Why this works: convolution in the time domain equals
 * pointwise multiplication in the frequency domain. FFT is the
 * fast way to move between the two domains.
 *
 * This is the same trick Python's int multiplication uses for
 * huge integers, and the way signal processing multiplies filters.
 *
 * Usage:
 *   ./fft_convolution
 *
 * Skills practiced:
 *   - Complex number arithmetic (from Day 53)
 *   - FFT and inverse FFT (from Day 53)
 *   - Zero-padding to avoid wraparound
 *   - Comparing against schoolbook multiplication
 *   - Floating-point tolerance in result comparison
 *   - Rounding FFT output back to integers
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

/* ---------- complex arithmetic (from Day 53) ---------- */

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

/* ---------- bit reversal (from Day 53) ---------- */

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

static unsigned log2_int(unsigned n)
{
    unsigned bits = 0;
    while ((1u << bits) < n)
        bits++;
    return bits;
}

/* ---------- FFT / IFFT (from Day 53) ---------- */

static void fft(complex_t *x, unsigned n, int inverse)
{
    unsigned bits = log2_int(n);

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

    for (unsigned len = 2; len <= n; len <<= 1)
    {
        double angle = 2.0 * M_PI / (double)len * (inverse ? 1.0 : -1.0);
        complex_t wlen = c_from_polar(1.0, angle);

        for (unsigned i = 0; i < n; i += len)
        {
            complex_t w = { 1.0, 0.0 };
            for (unsigned j = 0; j < len / 2; j++)
            {
                complex_t u = x[i + j];
                complex_t v = c_mul(w, x[i + j + len / 2]);
                x[i + j]           = c_add(u, v);
                x[i + j + len / 2] = c_sub(u, v);
                w = c_mul(w, wlen);
            }
        }
    }

    if (inverse)
    {
        for (unsigned i = 0; i < n; i++)
        {
            x[i].re /= (double)n;
            x[i].im /= (double)n;
        }
    }
}

/* ---------- schoolbook polynomial multiplication ---------- */

/* out[i+j] += a[i] * b[j], for all i, j.
 * Requires out to be zero-initialized and length >= a_len + b_len - 1. */
static void poly_mul_schoolbook(const double *a, unsigned a_len,
                                const double *b, unsigned b_len,
                                double *out)
{
    memset(out, 0, (a_len + b_len - 1) * sizeof(*out));
    for (unsigned i = 0; i < a_len; i++)
        for (unsigned j = 0; j < b_len; j++)
            out[i + j] += a[i] * b[j];
}

/* ---------- FFT-based polynomial multiplication ---------- */

/* Next power of two >= x */
static unsigned next_pow2(unsigned x)
{
    unsigned p = 1;
    while (p < x)
        p <<= 1;
    return p;
}

/* Multiply a * b via FFT. out must have space for at least
 * a_len + b_len - 1 doubles. */
static void poly_mul_fft(const double *a, unsigned a_len,
                         const double *b, unsigned b_len,
                         double *out)
{
    unsigned result_len = a_len + b_len - 1;
    unsigned N = next_pow2(result_len);

    /* Allocate complex arrays, zero-padded to length N */
    complex_t *fa = calloc(N, sizeof(*fa));
    complex_t *fb = calloc(N, sizeof(*fb));
    if (!fa || !fb)
    {
        fprintf(stderr, "calloc failed\n");
        exit(EXIT_FAILURE);
    }

    /* Copy real coefficients into the complex arrays */
    for (unsigned i = 0; i < a_len; i++) fa[i].re = a[i];
    for (unsigned i = 0; i < b_len; i++) fb[i].re = b[i];

    /* Forward FFTs */
    fft(fa, N, 0);
    fft(fb, N, 0);

    /* Pointwise multiplication */
    for (unsigned i = 0; i < N; i++)
        fa[i] = c_mul(fa[i], fb[i]);

    /* Inverse FFT */
    fft(fa, N, 1);

    /* Copy the real parts back. The imaginary parts should be
     * near zero (they're numerical noise); we discard them. */
    for (unsigned i = 0; i < result_len; i++)
        out[i] = fa[i].re;

    free(fa);
    free(fb);
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-55s %s\n", label, condition ? "ok" : "FAIL");
}

/* Compare two coefficient arrays within tolerance */
static int arrays_close(const double *a, const double *b,
                        unsigned len, double eps)
{
    for (unsigned i = 0; i < len; i++)
        if (fabs(a[i] - b[i]) > eps)
            return 0;
    return 1;
}

static void test_small(void)
{
    printf("Test 1: small polynomial multiplication\n");

    /* (1 + 2x + 3x^2) * (4 + 5x) = 4 + 13x + 22x^2 + 15x^3 */
    double a[] = { 1, 2, 3 };
    double b[] = { 4, 5 };
    double expected[] = { 4, 13, 22, 15 };

    unsigned a_len = 3, b_len = 2, result_len = 4;

    double *fft_out = calloc(result_len, sizeof(*fft_out));
    double *school_out = calloc(result_len, sizeof(*school_out));
    if (!fft_out || !school_out) exit(EXIT_FAILURE);

    poly_mul_fft(a, a_len, b, b_len, fft_out);
    poly_mul_schoolbook(a, a_len, b, b_len, school_out);

    check("FFT matches expected coefficients",
          arrays_close(fft_out, expected, result_len, 1e-9));
    check("schoolbook matches expected coefficients",
          arrays_close(school_out, expected, result_len, 1e-9));
    check("FFT agrees with schoolbook",
          arrays_close(fft_out, school_out, result_len, 1e-9));

    free(fft_out);
    free(school_out);
    printf("\n");
}

static void test_identity(void)
{
    printf("Test 2: multiplying by 1\n");

    /* p(x) * 1 = p(x) */
    double p[] = { 3, -1, 4, -1, 5 };
    double one[] = { 1 };
    unsigned p_len = 5, result_len = 5;

    double *out = calloc(result_len, sizeof(*out));
    if (!out) exit(EXIT_FAILURE);

    poly_mul_fft(p, p_len, one, 1, out);

    check("p * 1 == p", arrays_close(out, p, result_len, 1e-9));

    free(out);
    printf("\n");
}

static void test_random_comparison(void)
{
    printf("Test 3: random coefficients, FFT vs schoolbook\n");

    srand(12345);
    int all_match = 1;

    for (int trial = 0; trial < 20; trial++)
    {
        unsigned a_len = 5 + rand() % 20;
        unsigned b_len = 5 + rand() % 20;
        unsigned result_len = a_len + b_len - 1;

        double *a = malloc(a_len * sizeof(*a));
        double *b = malloc(b_len * sizeof(*b));
        double *fft_out = calloc(result_len, sizeof(*fft_out));
        double *school_out = calloc(result_len, sizeof(*school_out));
        if (!a || !b || !fft_out || !school_out) exit(EXIT_FAILURE);

        for (unsigned i = 0; i < a_len; i++)
            a[i] = (double)(rand() % 21 - 10);
        for (unsigned i = 0; i < b_len; i++)
            b[i] = (double)(rand() % 21 - 10);

        poly_mul_fft(a, a_len, b, b_len, fft_out);
        poly_mul_schoolbook(a, a_len, b, b_len, school_out);

        /* Coefficients up to ~100, tolerance 1e-6 is safe */
        if (!arrays_close(fft_out, school_out, result_len, 1e-6))
        {
            printf("    mismatch on trial %d (a_len=%u, b_len=%u)\n",
                   trial, a_len, b_len);
            all_match = 0;
        }

        free(a);
        free(b);
        free(fft_out);
        free(school_out);
    }

    check("FFT matches schoolbook on 20 random pairs", all_match);
    printf("\n");
}

static void test_large_polynomial(void)
{
    printf("Test 4: large polynomial (degree ~500)\n");

    unsigned n = 500;
    unsigned result_len = 2 * n - 1;

    double *a = malloc(n * sizeof(*a));
    double *b = malloc(n * sizeof(*b));
    double *fft_out = calloc(result_len, sizeof(*fft_out));
    double *school_out = calloc(result_len, sizeof(*school_out));
    if (!a || !b || !fft_out || !school_out) exit(EXIT_FAILURE);

    /* Small integer coefficients so results stay well-behaved */
    for (unsigned i = 0; i < n; i++)
    {
        a[i] = (double)((i * 7 + 1) % 11 - 5);
        b[i] = (double)((i * 3 + 2) % 11 - 5);
    }

    clock_t t0 = clock();
    poly_mul_schoolbook(a, n, b, n, school_out);
    clock_t t1 = clock();
    double school_ms = 1000.0 * (t1 - t0) / CLOCKS_PER_SEC;

    t0 = clock();
    poly_mul_fft(a, n, b, n, fft_out);
    t1 = clock();
    double fft_ms = 1000.0 * (t1 - t0) / CLOCKS_PER_SEC;

    printf("  n = %u, result length = %u\n", n, result_len);
    printf("  schoolbook: %.2f ms\n", school_ms);
    printf("  FFT:        %.2f ms\n", fft_ms);

    check("FFT result matches schoolbook within 1e-6",
          arrays_close(fft_out, school_out, result_len, 1e-6));

    free(a); free(b); free(fft_out); free(school_out);
    printf("\n");
}

static void test_huge_polynomial(void)
{
    printf("Test 5: huge polynomial (degree ~100,000) — FFT-only\n");

    unsigned n = 100000;
    unsigned result_len = 2 * n - 1;

    double *a = malloc(n * sizeof(*a));
    double *b = malloc(n * sizeof(*b));
    double *fft_out = calloc(result_len, sizeof(*fft_out));
    if (!a || !b || !fft_out) exit(EXIT_FAILURE);

    for (unsigned i = 0; i < n; i++)
    {
        a[i] = (double)(i % 5 - 2);
        b[i] = (double)(i % 3 - 1);
    }

    clock_t t0 = clock();
    poly_mul_fft(a, n, b, n, fft_out);
    clock_t t1 = clock();
    double fft_ms = 1000.0 * (t1 - t0) / CLOCKS_PER_SEC;

    printf("  n = %u, result length = %u\n", n, result_len);
    printf("  FFT time: %.2f ms (schoolbook would be ~%uM ops)\n",
           fft_ms, (n * n) / 1000000);

    /* Sanity: check a few coefficients against a spot calculation */
    /* c[0] = a[0]*b[0] */
    check("c[0] correct", fabs(fft_out[0] - a[0] * b[0]) < 1e-6);

    /* c[1] = a[0]*b[1] + a[1]*b[0] */
    check("c[1] correct",
          fabs(fft_out[1] - (a[0] * b[1] + a[1] * b[0])) < 1e-6);

    /* c[result_len-1] = a[n-1] * b[n-1] */
    check("last coefficient correct",
          fabs(fft_out[result_len - 1] - a[n - 1] * b[n - 1]) < 1e-6);

    free(a); free(b); free(fft_out);
    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("       FFT CONVOLUTION              \n");
    printf("====================================\n\n");

    test_small();
    test_identity();
    test_random_comparison();
    test_large_polynomial();
    test_huge_polynomial();

    printf("All tests complete.\n");
    return 0;
}

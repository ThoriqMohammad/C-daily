/* polynomial.c
 *
 * A small polynomial arithmetic toolkit for integer coefficients.
 *
 * A polynomial is a sum of terms:
 *
 *   p(x) = a0 + a1*x + a2*x^2 + ... + an*x^n
 *
 * We represent it as a fixed-size array of coefficients where
 * coeff[i] is the multiplier for x^i, plus a degree field that
 * tells us the highest non-zero term:
 *
 *   degree = 0   ->  constant      (e.g. 7)
 *   degree = 1   ->  linear        (e.g. 3x + 2)
 *   degree = 2   ->  quadratic     (e.g. x^2 - 5x + 6)
 *
 * The tools provided:
 *   - create from a list of coefficients
 *   - evaluate at a given x (Horner's method)
 *   - add, subtract
 *   - multiply (coefficient convolution)
 *   - print in human-readable form
 *
 * Limitations (deliberate, for a one-day project):
 *   - integer coefficients only
 *   - degree at most POLY_MAX_DEGREE
 *   - no division, factoring, or symbolic simplification
 *
 * Usage:
 *   ./polynomial
 *
 * Skills practiced:
 *   - Struct-based data types
 *   - Dynamic allocation and cleanup
 *   - Horner's method for fast evaluation
 *   - Nested loops for coefficient-wise multiplication
 *   - Trimming trailing zero coefficients (canonical form)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POLY_MAX_DEGREE 16

typedef struct
{
    long long coeff[POLY_MAX_DEGREE + 1]; /* index = power of x */
    int degree;                           /* highest non-zero   */
} polynomial;

/* ---------- construction ---------- */

/* Create a zero polynomial */
polynomial poly_zero(void)
{
    polynomial p;
    memset(&p, 0, sizeof(p));
    p.degree = 0;
    return p;
}

/* Create from a coefficient array where coeffs[0] is the constant term.
 * 'n' is the number of coefficients (not the degree). */
polynomial poly_from_coeffs(const long long *coeffs, int n)
{
    polynomial p = poly_zero();

    if (n <= 0)
        return p;

    if (n > POLY_MAX_DEGREE + 1)
        n = POLY_MAX_DEGREE + 1;

    for (int i = 0; i < n; i++)
        p.coeff[i] = coeffs[i];

    p.degree = n - 1;
    while (p.degree > 0 && p.coeff[p.degree] == 0)
        p.degree--;

    return p;
}

/* ---------- canonical form ---------- */

/* Strip trailing zero coefficients so degree reflects reality */
void poly_trim(polynomial *p)
{
    while (p->degree > 0 && p->coeff[p->degree] == 0)
        p->degree--;
}

/* ---------- evaluation ---------- */

/* Horner's method: p(x) = (...((a_n * x + a_{n-1}) * x + a_{n-2}) ... * x + a_0)
 * Requires n multiplications and n additions instead of n^2. */
long long poly_eval(const polynomial *p, long long x)
{
    long long result = p->coeff[p->degree];

    for (int i = p->degree - 1; i >= 0; i--)
        result = result * x + p->coeff[i];

    return result;
}

/* ---------- arithmetic ---------- */

polynomial poly_add(const polynomial *a, const polynomial *b)
{
    polynomial r = poly_zero();
    int max = (a->degree > b->degree) ? a->degree : b->degree;

    for (int i = 0; i <= max; i++)
    {
        long long ca = (i <= a->degree) ? a->coeff[i] : 0;
        long long cb = (i <= b->degree) ? b->coeff[i] : 0;
        r.coeff[i] = ca + cb;
    }

    r.degree = max;
    poly_trim(&r);
    return r;
}

polynomial poly_sub(const polynomial *a, const polynomial *b)
{
    polynomial r = poly_zero();
    int max = (a->degree > b->degree) ? a->degree : b->degree;

    for (int i = 0; i <= max; i++)
    {
        long long ca = (i <= a->degree) ? a->coeff[i] : 0;
        long long cb = (i <= b->degree) ? b->coeff[i] : 0;
        r.coeff[i] = ca - cb;
    }

    r.degree = max;
    poly_trim(&r);
    return r;
}

polynomial poly_mul(const polynomial *a, const polynomial *b)
{
    polynomial r = poly_zero();

    if (a->degree + b->degree > POLY_MAX_DEGREE)
    {
        fprintf(stderr, "poly_mul: result degree exceeds %d\n",
                POLY_MAX_DEGREE);
        return r;
    }

    /* Convolution: coeff[k] of the product is the sum of
     * a[i] * b[k-i] for all valid i. */
    for (int i = 0; i <= a->degree; i++)
        for (int j = 0; j <= b->degree; j++)
            r.coeff[i + j] += a->coeff[i] * b->coeff[j];

    r.degree = a->degree + b->degree;
    poly_trim(&r);
    return r;
}

/* ---------- printing ---------- */

void poly_print(const char *label, const polynomial *p)
{
    printf("%-4s = ", label);

    int printed = 0;

    for (int i = p->degree; i >= 0; i--)
    {
        long long c = p->coeff[i];
        if (c == 0)
            continue;

        if (printed && c > 0)
            printf(" + ");
        else if (printed && c < 0)
            printf(" - ");
        else if (c < 0)
            printf("-");

        long long mag = (c < 0) ? -c : c;

        if (i == 0)
            printf("%lld", mag);
        else if (i == 1)
            printf("%lldx", mag);
        else
            printf("%lldx^%d", mag, i);

        printed = 1;
    }

    if (!printed)
        printf("0");

    printf("\n");
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-44s %s\n", label, condition ? "ok" : "FAIL");
}

static void test_evaluation(void)
{
    printf("Test 1: evaluation with Horner's method\n");

    /* p(x) = x^2 - 5x + 6 = (x-2)(x-3) */
    long long c[] = {6, -5, 1};
    polynomial p = poly_from_coeffs(c, 3);

    check("p(0) = 6", poly_eval(&p, 0) == 6);
    check("p(2) = 0", poly_eval(&p, 2) == 0);
    check("p(3) = 0", poly_eval(&p, 3) == 0);
    check("p(10) = 56", poly_eval(&p, 10) == 56);

    printf("\n");
}

static void test_add_sub(void)
{
    printf("Test 2: addition and subtraction\n");

    long long ca[] = {1, 2, 3};   /* 3x^2 + 2x + 1 */
    long long cb[] = {4, 5};      /* 5x + 4        */

    polynomial a = poly_from_coeffs(ca, 3);
    polynomial b = poly_from_coeffs(cb, 2);

    polynomial s = poly_add(&a, &b);
    polynomial d = poly_sub(&a, &b);

    poly_print("a", &a);
    poly_print("b", &b);
    poly_print("a+b", &s);
    poly_print("a-b", &d);

    check("(a+b)(1) = 15", poly_eval(&s, 1) == 15);
    check("(a-b)(1) = 3",  poly_eval(&d, 1) == 3);

    printf("\n");
}

static void test_mul(void)
{
    printf("Test 3: multiplication\n");

    /* (x + 1) * (x + 2) = x^2 + 3x + 2 */
    long long ca[] = {1, 1};
    long long cb[] = {2, 1};

    polynomial a = poly_from_coeffs(ca, 2);
    polynomial b = poly_from_coeffs(cb, 2);
    polynomial p = poly_mul(&a, &b);

    poly_print("a", &a);
    poly_print("b", &b);
    poly_print("a*b", &p);

    check("degree of product is 2", p.degree == 2);
    check("product coeff[0] = 2", p.coeff[0] == 2);
    check("product coeff[1] = 3", p.coeff[1] == 3);
    check("product coeff[2] = 1", p.coeff[2] == 1);

    printf("\n");
}

static void test_trim(void)
{
    printf("Test 4: trimming leading zeros\n");

    /* 0x^3 + 0x^2 + 2x + 5  ->  should trim to 2x + 5 */
    long long c[] = {5, 2, 0, 0};
    polynomial p = poly_from_coeffs(c, 4);

    check("degree trimmed to 1", p.degree == 1);

    /* Multiplying by zero polynomial gives zero */
    polynomial z = poly_zero();
    polynomial r = poly_mul(&p, &z);

    check("multiply by zero is zero", r.degree == 0 && r.coeff[0] == 0);

    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("           POLYNOMIAL               \n");
    printf("====================================\n\n");

    test_evaluation();
    test_add_sub();
    test_mul();
    test_trim();

    printf("All tests complete.\n");
    return 0;
}

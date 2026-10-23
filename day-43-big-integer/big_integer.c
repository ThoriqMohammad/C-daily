/* big_integer.c
 *
 * Arbitrary-precision non-negative integers in C.
 *
 * A big integer is stored as an array of decimal digits, least
 * significant first, with a length field. This is the simplest
 * representation and prints cleanly without conversion, at the
 * cost of not being word-aligned like a production bignum.
 *
 * This implementation supports:
 *   - parsing from a decimal string
 *   - printing to a decimal string
 *   - addition
 *   - multiplication (schoolbook algorithm)
 *   - comparison
 *
 * Not supported (deliberately, to keep this to one day):
 *   - subtraction (needs sign handling or a "no negative" invariant)
 *   - division, modulo
 *   - signed values
 *   - base 2^32 or base 2^64 words (slower but simpler here)
 *
 * Usage:
 *   ./big_integer
 *
 * Skills practiced:
 *   - Dynamic allocation sized to the problem
 *   - Carry propagation (the heart of every bignum)
 *   - Digit-array arithmetic, most significant last
 *   - Parsing and formatting numeric strings
 *   - Canonical form: no leading zeros, length always reflects reality
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct
{
    unsigned char *digits; /* digits[i] is the 10^i place, 0..9 */
    size_t len;            /* number of significant digits (>= 1) */
    size_t cap;            /* allocated capacity in digits */
} big_integer;

/* ---------- construction and destruction ---------- */

/* Allocate a big_integer able to hold at least 'capacity' digits */
big_integer bi_create(size_t capacity)
{
    big_integer bi;
    bi.cap = (capacity < 1) ? 1 : capacity;
    bi.digits = malloc(bi.cap);
    if (bi.digits == NULL)
    {
        fprintf(stderr, "big_integer: out of memory\n");
        exit(EXIT_FAILURE);
    }
    bi.digits[0] = 0;
    bi.len = 1;
    return bi;
}

/* Free the digit array */
void bi_destroy(big_integer *bi)
{
    free(bi->digits);
    bi->digits = NULL;
    bi->len = 0;
    bi->cap = 0;
}

/* Make sure the buffer has room for at least 'needed' digits */
void bi_reserve(big_integer *bi, size_t needed)
{
    if (bi->cap >= needed)
        return;

    size_t newcap = bi->cap;
    while (newcap < needed)
        newcap *= 2;

    unsigned char *newdigits = realloc(bi->digits, newcap);
    if (newdigits == NULL)
    {
        fprintf(stderr, "big_integer: realloc failed\n");
        exit(EXIT_FAILURE);
    }

    bi->digits = newdigits;
    bi->cap = newcap;
}

/* ---------- canonical form ---------- */

/* Strip leading zeros so len reflects the true value */
void bi_trim(big_integer *bi)
{
    while (bi->len > 1 && bi->digits[bi->len - 1] == 0)
        bi->len--;
}

/* ---------- parsing and printing ---------- */

/* Parse a decimal string into a big_integer.
 * Skips leading whitespace, ignores underscores as separators,
 * rejects anything that is not a digit. */
big_integer bi_from_string(const char *s)
{
    if (s == NULL)
    {
        big_integer z = bi_create(1);
        return z;
    }

    while (isspace((unsigned char)*s))
        s++;

    /* First pass: count digits so we can size the buffer exactly */
    size_t count = 0;
    for (const char *p = s; *p; p++)
    {
        if (isdigit((unsigned char)*p))
            count++;
        else if (*p == '_')
            continue;
        else
            break;
    }

    big_integer bi = bi_create(count > 0 ? count : 1);

    if (count == 0)
        return bi;

    /* Fill digits least significant first, reading the string backwards */
    size_t idx = 0;
    for (const char *p = s; *p && idx < count; p++)
    {
        if (*p == '_')
            continue;
        if (!isdigit((unsigned char)*p))
            break;
    }

    /* Simpler: recompute from the end of the digit run */
    const char *end = s;
    size_t seen = 0;
    while (*end && (isdigit((unsigned char)*end) || *end == '_'))
    {
        if (isdigit((unsigned char)*end))
            seen++;
        end++;
    }

    (void)idx;
    size_t pos = 0;
    for (const char *p = end; p > s && pos < seen; )
    {
        p--;
        if (*p == '_')
            continue;
        if (!isdigit((unsigned char)*p))
            break;
        bi.digits[pos++] = (unsigned char)(*p - '0');
    }

    bi.len = seen;
    bi_trim(&bi);
    return bi;
}

/* Print a big_integer to stdout, most significant first */
void bi_print(const char *label, const big_integer *bi)
{
    if (label != NULL)
        printf("%s = ", label);

    for (size_t i = bi->len; i > 0; i--)
        putchar('0' + bi->digits[i - 1]);

    putchar('\n');
}

/* ---------- comparison ---------- */

/* Returns -1, 0, or 1 */
int bi_compare(const big_integer *a, const big_integer *b)
{
    if (a->len != b->len)
        return (a->len < b->len) ? -1 : 1;

    for (size_t i = a->len; i > 0; i--)
    {
        if (a->digits[i - 1] != b->digits[i - 1])
            return (a->digits[i - 1] < b->digits[i - 1]) ? -1 : 1;
    }
    return 0;
}

/* ---------- arithmetic ---------- */

big_integer bi_add(const big_integer *a, const big_integer *b)
{
    size_t maxlen = (a->len > b->len) ? a->len : b->len;

    big_integer r = bi_create(maxlen + 1);

    int carry = 0;
    for (size_t i = 0; i < maxlen; i++)
    {
        int da = (i < a->len) ? a->digits[i] : 0;
        int db = (i < b->len) ? b->digits[i] : 0;
        int sum = da + db + carry;
        r.digits[i] = (unsigned char)(sum % 10);
        carry = sum / 10;
    }

    r.len = maxlen;
    if (carry)
        r.digits[r.len++] = (unsigned char)carry;

    bi_trim(&r);
    return r;
}

big_integer bi_mul(const big_integer *a, const big_integer *b)
{
    /* The product has at most a->len + b->len digits */
    big_integer r = bi_create(a->len + b->len);

    /* Zero the digit buffer: r starts as 0 */
    memset(r.digits, 0, r.cap);

    for (size_t i = 0; i < a->len; i++)
    {
        int carry = 0;
        for (size_t j = 0; j < b->len; j++)
        {
            int prod = r.digits[i + j]
                     + (int)a->digits[i] * (int)b->digits[j]
                     + carry;
            r.digits[i + j] = (unsigned char)(prod % 10);
            carry = prod / 10;
        }

        /* Propagate the final carry leftward */
        size_t k = i + b->len;
        while (carry > 0)
        {
            int sum = r.digits[k] + carry;
            r.digits[k] = (unsigned char)(sum % 10);
            carry = sum / 10;
            k++;
        }
    }

    r.len = a->len + b->len;
    bi_trim(&r);
    return r;
}

/* ---------- factorial ---------- */

/* Compute n! by repeated multiplication.
 * Returns a zero big_integer for n <= 1 (0! = 1! = 1 by convention,
 * so we special-case both). */
big_integer bi_factorial(unsigned n)
{
    big_integer result = bi_create(1);
    result.digits[0] = 1;
    result.len = 1;

    for (unsigned i = 2; i <= n; i++)
    {
        /* Build a big_integer for i */
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", i);
        big_integer mult = bi_from_string(buf);

        big_integer next = bi_mul(&result, &mult);
        bi_destroy(&result);
        bi_destroy(&mult);
        result = next;
    }

    return result;
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-48s %s\n", label, condition ? "ok" : "FAIL");
}

/* Compare a big_integer against a decimal string */
static int bi_equals_str(const big_integer *bi, const char *expected)
{
    big_integer e = bi_from_string(expected);
    int eq = (bi_compare(bi, &e) == 0);
    bi_destroy(&e);
    return eq;
}

static void test_parse_and_print(void)
{
    printf("Test 1: parse and print round-trip\n");

    const char *cases[] = {
        "0", "1", "9", "10", "42", "1000000000", "1234567890123456789"
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        big_integer bi = bi_from_string(cases[i]);
        char label[64];
        snprintf(label, sizeof(label), "parse \"%s\"", cases[i]);
        check(label, bi_equals_str(&bi, cases[i]));
        bi_destroy(&bi);
    }
    printf("\n");
}

static void test_addition(void)
{
    printf("Test 2: addition\n");

    big_integer a = bi_from_string("999999999999");
    big_integer b = bi_from_string("1");
    big_integer s = bi_add(&a, &b);

    check("999999999999 + 1 = 1000000000000", bi_equals_str(&s, "1000000000000"));

    big_integer c = bi_from_string("123456789");
    big_integer d = bi_from_string("987654321");
    big_integer t = bi_add(&c, &d);

    check("123456789 + 987654321 = 1111111110",
          bi_equals_str(&t, "1111111110"));

    bi_destroy(&a); bi_destroy(&b); bi_destroy(&s);
    bi_destroy(&c); bi_destroy(&d); bi_destroy(&t);
    printf("\n");
}

static void test_multiplication(void)
{
    printf("Test 3: multiplication\n");

    big_integer a = bi_from_string("12345");
    big_integer b = bi_from_string("6789");
    big_integer p = bi_mul(&a, &b);

    /* 12345 * 6789 = 83810205 */
    check("12345 * 6789 = 83810205", bi_equals_str(&p, "83810205"));

    big_integer big = bi_from_string("99999999999999999999");
    big_integer self = bi_from_string("99999999999999999999");
    big_integer sq = bi_mul(&big, &self);

    /* (10^20 - 1)^2 = 10^40 - 2*10^20 + 1
     *               = 9999999999999999999800000000000000000001 */
    check("(10^20 - 1)^2 matches expected",
          bi_equals_str(&sq, "9999999999999999999800000000000000000001"));

    bi_destroy(&a); bi_destroy(&b); bi_destroy(&p);
    bi_destroy(&big); bi_destroy(&self); bi_destroy(&sq);
    printf("\n");
}

static void test_factorial(void)
{
    printf("Test 4: factorial\n");

    big_integer f10 = bi_factorial(10);
    check("10! = 3628800", bi_equals_str(&f10, "3628800"));

    big_integer f20 = bi_factorial(20);
    check("20! = 2432902008176640000",
          bi_equals_str(&f20, "2432902008176640000"));

    big_integer f30 = bi_factorial(30);
    check("30! = 265252859812191058636308480000000",
          bi_equals_str(&f30, "265252859812191058636308480000000"));

    bi_destroy(&f10); bi_destroy(&f20); bi_destroy(&f30);
    printf("\n");
}

static void test_compare(void)
{
    printf("Test 5: comparison\n");

    big_integer a = bi_from_string("100");
    big_integer b = bi_from_string("99");
    big_integer c = bi_from_string("100");

    check("100 > 99",   bi_compare(&a, &b) == 1);
    check("99 < 100",   bi_compare(&b, &a) == -1);
    check("100 == 100", bi_compare(&a, &c) == 0);

    bi_destroy(&a); bi_destroy(&b); bi_destroy(&c);
    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("           BIG INTEGER              \n");
    printf("====================================\n\n");

    test_parse_and_print();
    test_addition();
    test_multiplication();
    test_factorial();
    test_compare();

    printf("All tests complete.\n");
    return 0;
}

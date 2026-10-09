/* complex_numbers.c
 *
 * A complex number library in C.
 *
 * A complex number has the form a + bi, where:
 *   - a is the real part
 *   - b is the imaginary part
 *   - i is the imaginary unit (i² = -1)
 *
 * Supports:
 *   - Create / add / subtract / multiply / divide
 *   - Magnitude (modulus)
 *   - Conjugate
 *   - Power
 *   - Comparison
 *   - Polar form
 *
 * Usage:
 *   ./complex_numbers
 *
 * Skills practiced:
 *   - Structs
 *   - Math (sqrt, atan2, cos, sin, pow)
 *   - Complex arithmetic
 *   - Function design
 *   - Return by value
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#define EPSILON 1e-9

/* A complex number */
typedef struct {
    double real;
    double imag;
} Complex;

/* --- Creation --- */
Complex complex_make(double real, double imag)
{
    Complex c;
    c.real = real;
    c.imag = imag;
    return c;
}

/* --- Arithmetic --- */
Complex complex_add(Complex a, Complex b)
{
    return complex_make(a.real + b.real, a.imag + b.imag);
}

Complex complex_sub(Complex a, Complex b)
{
    return complex_make(a.real - b.real, a.imag - b.imag);
}

Complex complex_mul(Complex a, Complex b)
{
    /* (a + bi)(c + di) = (ac - bd) + (ad + bc)i */
    return complex_make(a.real * b.real - a.imag * b.imag,
                        a.real * b.imag + a.imag * b.real);
}

Complex complex_div(Complex a, Complex b)
{
    /* (a + bi)/(c + di) = ((ac + bd) + (bc - ad)i) / (c² + d²) */
    double denom = b.real * b.real + b.imag * b.imag;
    if (fabs(denom) < EPSILON)
    {
        fprintf(stderr, "Error: division by zero.\n");
        exit(EXIT_FAILURE);
    }
    return complex_make((a.real * b.real + a.imag * b.imag) / denom,
                        (a.imag * b.real - a.real * b.imag) / denom);
}

/* --- Properties --- */
double complex_magnitude(Complex c)
{
    return sqrt(c.real * c.real + c.imag * c.imag);
}

Complex complex_conjugate(Complex c)
{
    return complex_make(c.real, -c.imag);
}

/* --- Power (integer exponent) --- */
Complex complex_pow(Complex base, int exp)
{
    Complex result = complex_make(1, 0);
    for (int i = 0; i < exp; i++)
        result = complex_mul(result, base);
    return result;
}

/* --- Polar form --- */
void complex_polar(Complex c, double *r, double *theta)
{
    *r = complex_magnitude(c);
    *theta = atan2(c.imag, c.real);
}

/* --- Comparison --- */
bool complex_equal(Complex a, Complex b)
{
    return fabs(a.real - b.real) < EPSILON &&
           fabs(a.imag - b.imag) < EPSILON;
}

/* --- Printing --- */
void complex_print(const char *label, Complex c)
{
    if (fabs(c.imag) < EPSILON)
        printf("%s = %.4f\n", label, c.real);
    else if (fabs(c.real) < EPSILON)
        printf("%s = %.4fi\n", label, c.imag);
    else if (c.imag > 0)
        printf("%s = %.4f + %.4fi\n", label, c.real, c.imag);
    else
        printf("%s = %.4f - %.4fi\n", label, c.real, -c.imag);
}

int main(void)
{
    printf("====================================\n");
    printf("         COMPLEX NUMBERS            \n");
    printf("====================================\n\n");

    Complex a = complex_make(3, 4);
    Complex b = complex_make(1, -2);

    complex_print("a", a);
    complex_print("b", b);
    printf("\n");

    complex_print("a + b", complex_add(a, b));
    complex_print("a - b", complex_sub(a, b));
    complex_print("a * b", complex_mul(a, b));
    complex_print("a / b", complex_div(a, b));
    printf("\n");

    printf("|a|      = %.4f\n", complex_magnitude(a));
    printf("|b|      = %.4f\n", complex_magnitude(b));
    complex_print("conj(a)", complex_conjugate(a));
    complex_print("conj(b)", complex_conjugate(b));
    printf("\n");

    complex_print("a^2", complex_pow(a, 2));
    complex_print("b^3", complex_pow(b, 3));
    printf("\n");

    double r, theta;
    complex_polar(a, &r, &theta);
    printf("a in polar form: r = %.4f, θ = %.4f rad\n", r, theta);

    printf("a == b? %s\n", complex_equal(a, b) ? "true" : "false");
    printf("a == a? %s\n", complex_equal(a, a) ? "true" : "false");

    return 0;
}

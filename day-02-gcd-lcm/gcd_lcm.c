/* gcd_lcm.c
 *
 * Computes the Greatest Common Divisor (GCD) and
 * Least Common Multiple (LCM) of two integers.
 *
 * Uses the Euclidean algorithm (recursive) for GCD.
 * LCM is derived as: (a / gcd) * b   — avoids overflow.
 *
 * Usage:
 *   ./gcd_lcm a b
 *
 * Example:
 *   ./gcd_lcm 48 18
 *   GCD(48, 18) = 6
 *   LCM(48, 18) = 144
 */

#include <stdio.h>
#include <stdlib.h>

/* Recursive Euclidean algorithm */
int gcd(int a, int b)
{
    if (b == 0)
        return a;
    return gcd(b, a % b);
}

/* LCM using the relation: lcm(a,b) = (a / gcd) * b */
int lcm(int a, int b)
{
    return (a / gcd(a, b)) * b;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s a b\n", argv[0]);
        return 1;
    }

    int a = atoi(argv[1]);
    int b = atoi(argv[2]);

    if (a <= 0 || b <= 0)
    {
        fprintf(stderr, "Error: both numbers must be positive integers.\n");
        return 1;
    }

    printf("GCD(%d, %d) = %d\n", a, b, gcd(a, b));
    printf("LCM(%d, %d) = %d\n", a, b, lcm(a, b));

    return 0;
}

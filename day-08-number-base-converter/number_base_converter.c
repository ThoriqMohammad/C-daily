/* number_base_converter.c
 *
 * Converts numbers between bases: binary, octal, decimal, hexadecimal.
 *
 * Usage:
 *   ./number_base_converter <value> <from_base> <to_base>
 *
 * Bases: 2 (binary), 8 (octal), 10 (decimal), 16 (hex)
 *
 * Examples:
 *   ./number_base_converter 255 10 2      -> 11111111
 *   ./number_base_converter 11111111 2 10 -> 255
 *   ./number_base_converter 255 10 16     -> FF
 *   ./number_base_converter FF 16 10      -> 255
 *
 * Skills practiced:
 *   - Number systems (binary, octal, decimal, hex)
 *   - String parsing
 *   - Modular arithmetic
 *   - Character arithmetic
 *   - argc/argv
 *   - Input validation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Convert a single character to its integer value in the given base */
int char_to_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;  /* Invalid */
}

/* Convert a string in the given base to a decimal (long) value */
long parse_in_base(const char *str, int base)
{
    long result = 0;
    int i = 0;

    /* Skip optional leading whitespace */
    while (isspace(str[i]))
        i++;

    /* Handle negative sign */
    int negative = 0;
    if (str[i] == '-')
    {
        negative = 1;
        i++;
    }

    for (; str[i] != '\0'; i++)
    {
        int digit = char_to_value(str[i]);

        if (digit < 0 || digit >= base)
        {
            fprintf(stderr, "Error: '%c' is not a valid digit in base %d.\n",
                    str[i], base);
            exit(EXIT_FAILURE);
        }

        result = result * base + digit;
    }

    return negative ? -result : result;
}

/* Convert a decimal value to a string in the given base */
void convert_to_base(long value, int base, char *out, int size)
{
    char temp[64];
    int i = 0;
    int negative = 0;

    if (value == 0)
    {
        snprintf(out, size, "0");
        return;
    }

    if (value < 0)
    {
        negative = 1;
        value = -value;
    }

    while (value > 0)
    {
        int digit = value % base;
        temp[i++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        value /= base;
    }

    if (negative)
        temp[i++] = '-';

    /* Reverse into out */
    int j;
    for (j = 0; j < i; j++)
        out[j] = temp[i - 1 - j];
    out[j] = '\0';
}

/* Check if a base is valid */
int valid_base(int base)
{
    return base == 2 || base == 8 || base == 10 || base == 16;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <value> <from_base> <to_base>\n", argv[0]);
        fprintf(stderr, "  Bases: 2, 8, 10, 16\n");
        return 1;
    }

    const char *value_str = argv[1];
    int from_base = atoi(argv[2]);
    int to_base = atoi(argv[3]);

    if (!valid_base(from_base) || !valid_base(to_base))
    {
        fprintf(stderr, "Error: bases must be 2, 8, 10, or 16.\n");
        return 1;
    }

    long value = parse_in_base(value_str, from_base);

    char output[64];
    convert_to_base(value, to_base, output, sizeof(output));

    printf("%s\n", output);
    return 0;
}

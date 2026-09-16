/* caesar_cipher.c
 *
 * Encrypts and decrypts messages using the Caesar cipher.
 *
 * The Caesar cipher shifts each letter by a fixed number
 * of positions in the alphabet. For example, with a shift
 * of 3:
 *
 *   A -> D
 *   B -> E
 *   ...
 *   X -> A  (wraps around)
 *   Y -> B
 *   Z -> C
 *
 * Non-letter characters are left unchanged.
 * Case is preserved (uppercase stays uppercase,
 * lowercase stays lowercase).
 *
 * Usage:
 *   ./caesar_cipher encrypt 3 "Hello, World!"
 *   ./caesar_cipher decrypt 3 "Khoor, Zruog!"
 *
 * Skills practiced:
 *   - Command-line arguments (argc/argv)
 *   - String traversal
 *   - Modular arithmetic
 *   - Character classification (isalpha, isupper, islower)
 *   - Classical cryptography
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Shift a single character by 'shift' positions */
char shift_char(char c, int shift)
{
    if (!isalpha(c))
        return c;

    /* Normalize shift to 0..25 */
    shift = ((shift % 26) + 26) % 26;

    if (isupper(c))
        return 'A' + (c - 'A' + shift) % 26;
    else
        return 'a' + (c - 'a' + shift) % 26;
}

/* Encrypt or decrypt a string in place */
void caesar(char *text, int shift, int decrypt)
{
    if (decrypt)
        shift = -shift;

    for (int i = 0; text[i] != '\0'; i++)
        text[i] = shift_char(text[i], shift);
}

/* Print usage */
void usage(const char *program)
{
    fprintf(stderr, "Usage: %s encrypt|decrypt shift \"message\"\n", program);
    fprintf(stderr, "  shift: integer (positive or negative)\n");
    fprintf(stderr, "  message: text in double quotes\n");
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        usage(argv[0]);
        return 1;
    }

    const char *mode = argv[1];
    int shift = atoi(argv[2]);
    char *message = argv[3];

    int decrypt;

    if (strcmp(mode, "encrypt") == 0)
        decrypt = 0;
    else if (strcmp(mode, "decrypt") == 0)
        decrypt = 1;
    else
    {
        fprintf(stderr, "Error: mode must be 'encrypt' or 'decrypt'.\n");
        usage(argv[0]);
        return 1;
    }

    caesar(message, shift, decrypt);

    printf("%s\n", message);
    return 0;
}

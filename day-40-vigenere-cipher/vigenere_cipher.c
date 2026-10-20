/* vigenere_cipher.c
 *
 * Encrypts and decrypts messages using the Vigenère cipher.
 *
 * The Vigenère cipher is a polyalphabetic substitution: instead
 * of shifting every letter by the same amount (as in the Caesar
 * cipher), it shifts each letter by a different amount taken from
 * a repeating keyword.
 *
 * For example, with the key "LEMON":
 *
 *   plaintext:  ATTACKATDAWN
 *   key:        LEMONLEMONLE
 *   ciphertext: LXFOPVEFRNHR
 *
 * Each letter of the key tells us how far to shift the
 * corresponding plaintext letter. When the key runs out, it
 * repeats from the beginning.
 *
 * This defeats the simple frequency analysis that breaks Caesar,
 * because the same plaintext letter encrypts differently depending
 * on its position in the key cycle.
 *
 * Usage:
 *   ./vigenere_cipher encrypt KEY "message"
 *   ./vigenere_cipher decrypt KEY "ciphertext"
 *
 * Skills practiced:
 *   - Poly-key arithmetic (each letter shifted by a different amount)
 *   - Modular arithmetic and character classification
 *   - Key validation (must be alphabetic, non-empty)
 *   - In-place string transformation
 *   - Case preservation through a non-trivial cipher
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Check that the key is non-empty and all alphabetic.
 * Returns 1 if valid, 0 otherwise. */
int key_is_valid(const char *key)
{
    if (key == NULL || key[0] == '\0')
        return 0;

    for (int i = 0; key[i] != '\0'; i++)
        if (!isalpha((unsigned char)key[i]))
            return 0;

    return 1;
}

/* Convert a key letter to its shift value in 0..25 */
int key_shift(char k)
{
    return tolower((unsigned char)k) - 'a';
}

/* Shift one alphabetic character by 'shift' positions.
 * Non-letters are returned unchanged. */
char shift_char(char c, int shift)
{
    if (!isalpha((unsigned char)c))
        return c;

    shift = ((shift % 26) + 26) % 26;

    if (isupper((unsigned char)c))
        return 'A' + (c - 'A' + shift) % 26;
    else
        return 'a' + (c - 'a' + shift) % 26;
}

/* Encrypt or decrypt text in place using the given key.
 * The key only advances on alphabetic characters, so the
 * ciphertext length matches the plaintext length. */
void vigenere(char *text, const char *key, int decrypt)
{
    int keylen = (int)strlen(key);
    int keypos = 0;

    for (int i = 0; text[i] != '\0'; i++)
    {
        if (isalpha((unsigned char)text[i]))
        {
            int shift = key_shift(key[keypos % keylen]);
            if (decrypt)
                shift = -shift;
            text[i] = shift_char(text[i], shift);
            keypos++;
        }
    }
}

/* Print usage */
void usage(const char *program)
{
    fprintf(stderr, "Usage: %s encrypt|decrypt KEY \"message\"\n", program);
    fprintf(stderr, "  KEY: alphabetic keyword (non-empty)\n");
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
    const char *key = argv[2];
    char *message = argv[3];

    if (!key_is_valid(key))
    {
        fprintf(stderr, "Error: key must be non-empty and alphabetic.\n");
        return 1;
    }

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

    vigenere(message, key, decrypt);
    printf("%s\n", message);

    return 0;
}

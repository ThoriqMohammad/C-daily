/* xor_cipher.c
 *
 * Encrypts and decrypts data using the XOR cipher.
 *
 * XOR is a simple symmetric cipher: the same operation
 * both encrypts and decrypts.
 *
 *   ciphertext = plaintext  ^ key
 *   plaintext  = ciphertext ^ key
 *
 * The key is a single byte (0–255), applied to every byte.
 *
 * Usage:
 *   ./xor_cipher encrypt <key> <file>
 *   ./xor_cipher decrypt <key> <file>
 *
 * Or with stdin:
 *   echo "hello" | ./xor_cipher encrypt 42 -
 *
 * Skills practiced:
 *   - Bitwise XOR (^)
 *   - File I/O (binary mode)
 *   - Character/byte processing
 *   - Symmetric encryption concept
 *   - argc/argv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define BUF_SIZE 4096

/* XOR every byte of the input stream with the key */
void xor_stream(FILE *in, FILE *out, unsigned char key)
{
    unsigned char buf[BUF_SIZE];
    size_t n;

    while ((n = fread(buf, 1, BUF_SIZE, in)) > 0) {
        for (size_t i = 0; i < n; i++)
            buf[i] ^= key;
        if (fwrite(buf, 1, n, out) != n) {
            fprintf(stderr, "Error writing output: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "Error reading input: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s encrypt|decrypt <key> <file>\n", prog);
    fprintf(stderr, "  key:  0–255 (integer)\n");
    fprintf(stderr, "  file: input file, or '-' for stdin\n");
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        usage(argv[0]);
        return 1;
    }

    const char *mode = argv[1];
    int key = atoi(argv[2]);
    const char *filename = argv[3];

    if (strcmp(mode, "encrypt") != 0 && strcmp(mode, "decrypt") != 0) {
        fprintf(stderr, "Error: mode must be 'encrypt' or 'decrypt'.\n");
        usage(argv[0]);
        return 1;
    }

    if (key < 0 || key > 255) {
        fprintf(stderr, "Error: key must be between 0 and 255.\n");
        return 1;
    }

    /* Note: encrypt and decrypt are the same operation */
    FILE *in;
    if (strcmp(filename, "-") == 0)
        in = stdin;
    else {
        in = fopen(filename, "rb");
        if (in == NULL) {
            fprintf(stderr, "Error opening '%s': %s\n", filename, strerror(errno));
            return 1;
        }
    }

    xor_stream(in, stdout, (unsigned char) key);

    if (in != stdin)
        fclose(in);

    return 0;
}

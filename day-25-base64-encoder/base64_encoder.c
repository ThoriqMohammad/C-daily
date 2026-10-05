/* base64_encoder.c
 *
 * Base64 encoding and decoding in C.
 *
 * Base64 maps binary data to a 64-character alphabet
 * (A-Z, a-z, 0-9, +, /) plus '=' for padding.
 *
 * It's used everywhere:
 *   - Email attachments (MIME)
 *   - Data URLs (HTML/CSS)
 *   - JSON Web Tokens (JWT)
 *   - HTTP Basic Auth
 *   - Storing binary in text formats
 *
 * Usage:
 *   ./base64_encoder encode "Hello, world!"
 *   ./base64_encoder decode "SGVsbG8sIHdvcmxkIQ=="
 *
 * Skills practiced:
 *   - Bit manipulation
 *   - Lookup tables
 *   - String handling
 *   - Encoding/decoding
 *   - Security concepts
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const char B64_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Reverse lookup: character -> 6-bit value */
static int8_t B64_REVERSE[256];

/* Build the reverse lookup table */
static void build_reverse_table(void)
{
    memset(B64_REVERSE, -1, sizeof(B64_REVERSE));

    for (int i = 0; i < 64; i++)
        B64_REVERSE[(unsigned char) B64_ALPHABET[i]] = (int8_t) i;
}

/* Encode binary data to Base64 */
char *base64_encode(const unsigned char *data, size_t len)
{
    size_t out_len = 4 * ((len + 2) / 3);
    char *out = malloc(out_len + 1);
    if (out == NULL) return NULL;

    size_t i = 0, j = 0;

    while (i < len)
    {
        uint32_t octet_a = (i < len) ? data[i++] : 0;
        uint32_t octet_b = (i < len) ? data[i++] : 0;
        uint32_t octet_c = (i < len) ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out[j++] = B64_ALPHABET[(triple >> 18) & 0x3F];
        out[j++] = B64_ALPHABET[(triple >> 12) & 0x3F];
        out[j++] = B64_ALPHABET[(triple >> 6) & 0x3F];
        out[j++] = B64_ALPHABET[triple & 0x3F];
    }

    /* Add padding */
    size_t mod = len % 3;
    if (mod == 1)
    {
        out[out_len - 1] = '=';
        out[out_len - 2] = '=';
    }
    else if (mod == 2)
    {
        out[out_len - 1] = '=';
    }

    out[out_len] = '\0';
    return out;
}

/* Decode Base64 to binary */
unsigned char *base64_decode(const char *input, size_t *out_len)
{
    size_t in_len = strlen(input);
    if (in_len % 4 != 0) return NULL;

    size_t max_out = (in_len / 4) * 3;
    unsigned char *out = malloc(max_out + 1);
    if (out == NULL) return NULL;

    size_t i = 0, j = 0;

    while (i < in_len)
    {
        uint32_t sextet_a = B64_REVERSE[(unsigned char) input[i++]];
        uint32_t sextet_b = B64_REVERSE[(unsigned char) input[i++]];
        uint32_t sextet_c = B64_REVERSE[(unsigned char) input[i++]];
        uint32_t sextet_d = B64_REVERSE[(unsigned char) input[i++]];

        uint32_t triple = (sextet_a << 18) | (sextet_b << 12) |
                          (sextet_c << 6)  | sextet_d;

        if (j < max_out) out[j++] = (triple >> 16) & 0xFF;
        if (j < max_out) out[j++] = (triple >> 8) & 0xFF;
        if (j < max_out) out[j++] = triple & 0xFF;
    }

    /* Adjust for padding */
    if (in_len >= 1 && input[in_len - 1] == '=') j--;
    if (in_len >= 2 && input[in_len - 2] == '=') j--;

    out[j] = '\0';
    *out_len = j;
    return out;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s encode|decode <string>\n", argv[0]);
        return 1;
    }

    build_reverse_table();

    const char *mode = argv[1];
    const char *input = argv[2];

    if (strcmp(mode, "encode") == 0)
    {
        char *encoded = base64_encode((const unsigned char *) input, strlen(input));
        if (encoded == NULL)
        {
            fprintf(stderr, "Error: encoding failed.\n");
            return 1;
        }
        printf("%s\n", encoded);
        free(encoded);
    }
    else if (strcmp(mode, "decode") == 0)
    {
        size_t out_len;
        unsigned char *decoded = base64_decode(input, &out_len);
        if (decoded == NULL)
        {
            fprintf(stderr, "Error: decoding failed.\n");
            return 1;
        }

        printf("%s\n", decoded);
        free(decoded);
    }
    else
    {
        fprintf(stderr, "Error: mode must be 'encode' or 'decode'.\n");
        return 1;
    }

    return 0;
}

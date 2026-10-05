# Day 25 — base64_encoder

Base64 encoding and decoding in C.

## What Is Base64?

Base64 maps **binary data** to a **text alphabet** of 64 characters:

    A-Z  a-z  0-9  +  /

Every 3 bytes (24 bits) become 4 characters (4 × 6 bits).
If the input isn't a multiple of 3, `=` is added for padding.

## Examples

| Input | Base64 |
|-------|--------|
| `"Hello"` | `SGVsbG8=` |
| `"Hello, world!"` | `SGVsbG8sIHdvcmxkIQ==` |
| `"a"` | `YQ==` |
| `"ab"` | `YWI=` |
| `"abc"` | `YWJj` |

## Usage

    ./base64_encoder encode "Hello, world!"
    ./base64_encoder decode "SGVsbG8sIHdvcmxkIQ=="

## Build

    gcc -Wall -Wextra -std=c99 -o base64_encoder base64_encoder.c

## Skills Practiced

- **Bit manipulation** — shifts, masks
- **Lookup tables** — forward and reverse
- **String handling** — padding, length
- **Encoding/decoding** — symmetric operations
- **Security concepts** — where Base64 appears

## How It Works

### Encoding

Take 3 bytes, treat them as one 24-bit number, split into
four 6-bit values:

    "Man" → 0x4D 0x61 0x6E → 010011 010110 000101 101110
          → T      W      F      u
          → "TWFu"

### Decoding

Reverse the process: take 4 characters, look up their 6-bit
values, combine into 3 bytes.

## The Reverse Table

Building the reverse lookup:

    for (int i = 0; i < 64; i++)
        B64_REVERSE[(unsigned char) B64_ALPHABET[i]] = i;

This gives O(1) lookups during decoding.

## Where Base64 Is Used

| Domain | Example |
|--------|---------|
| **Email** | MIME attachments |
| **Web** | Data URLs (`data:image/png;base64,...`) |
| **Auth** | HTTP Basic Auth |
| **JWT** | JSON Web Tokens |
| **SSH** | Public key encoding |
| **APIs** | Binary payloads in JSON |

## Important: Base64 Is NOT Encryption

| Property | Base64 | Encryption |
|----------|--------|------------|
| **Purpose** | Encoding | Confidentiality |
| **Reversible?** | ✅ Anyone | ✅ With key |
| **Secure?** | ❌ No | ✅ Yes |

**Base64 provides no security. It's just a representation.**

## Notes

- Input length must be a multiple of 4 for decoding.
- Padding with `=` is required for valid Base64.
- The reverse table is built once at startup.
- Works on any binary data, not just text.

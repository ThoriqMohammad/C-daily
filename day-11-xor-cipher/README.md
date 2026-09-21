# Day 11 — xor_cipher

Encrypts and decrypts data using the **XOR cipher**—the simplest
symmetric cipher.

## How It Works

XOR has a beautiful property:

    plaintext  ^ key = ciphertext
    ciphertext ^ key = plaintext

So encryption and decryption are the **same operation**.

## Usage

    ./xor_cipher encrypt <key> <file>
    ./xor_cipher decrypt <key> <file>

- `key` is an integer from 0 to 255.
- `file` is the input file, or `-` for stdin.

## Examples

**Encrypt a file:**

    $ ./xor_cipher encrypt 42 message.txt > encrypted.bin

**Decrypt it back:**

    $ ./xor_cipher decrypt 42 encrypted.bin
    (prints the original message)

**With stdin:**

    $ echo "Hello, world!" | ./xor_cipher encrypt 42 | ./xor_cipher decrypt 42
    Hello, world!

## Build

    gcc -o xor_cipher xor_cipher.c

## Skills Practiced

- Bitwise XOR (`^`)
- Binary file I/O (`fread`, `fwrite`, `"rb"`)
- Byte-level processing
- Symmetric encryption concept
- Error handling (`errno`, `strerror`)
- `argc` / `argv`

## Why XOR Works

The XOR truth table:

    0 ^ 0 = 0
    0 ^ 1 = 1
    1 ^ 0 = 1
    1 ^ 1 = 0

XOR is its own inverse: `(x ^ k) ^ k = x`.

That's why the same code both encrypts and decrypts.

## Security Notes

- **XOR with a single-byte key is NOT secure.** It's easily broken
  by frequency analysis or brute force (only 256 keys).
- Real stream ciphers (RC4, ChaCha20) use a **keystream**, not
  a single byte.
- This project is for **learning**, not for protecting secrets.

## Notes

- The key is normalized to 0–255.
- Output is binary—pipe it to a file, not the terminal.
- Works on any file type (text, images, binaries).

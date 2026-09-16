# Day 07 — caesar_cipher

Encrypts and decrypts messages using the **Caesar cipher**—one of the
oldest known encryption techniques, attributed to Julius Caesar.

## How It Works

The Caesar cipher shifts each letter by a fixed number of positions
in the alphabet. For example, with a shift of 3:

    A -> D
    B -> E
    ...
    X -> A  (wraps around)
    Y -> B
    Z -> C

- Non-letter characters are left unchanged.
- Case is preserved (uppercase stays uppercase).
- Decryption is just encryption with a negative shift.

## Usage

    ./caesar_cipher encrypt shift "message"
    ./caesar_cipher decrypt shift "message"

`shift` can be any integer (positive or negative).

## Examples

**Encrypt:**

    $ ./caesar_cipher encrypt 3 "Hello, World!"
    Khoor, Zruog!

**Decrypt:**

    $ ./caesar_cipher decrypt 3 "Khoor, Zruog!"
    Hello, World!

**Negative shift:**

    $ ./caesar_cipher encrypt -3 "Hello, World!"
    Ebiil, Tloia!

**Large shift (wraps):**

    $ ./caesar_cipher encrypt 29 "abc"
    def

## Build

    gcc -o caesar_cipher caesar_cipher.c

## Skills Practiced

- Command-line arguments (`argc` / `argv`)
- String traversal
- **Modular arithmetic** — the heart of the Caesar cipher
- Character classification (`isalpha`, `isupper`, `islower`)
- Classical cryptography
- Input validation and error handling

## Math Behind the Cipher

The Caesar cipher is **modular arithmetic** in disguise:

    encrypted = (plain + shift) mod 26

The `% 26` is what makes `Z` wrap around to `A`.

## Notes

- The shift is normalized to `0..25` so any integer works.
- The message is modified **in place** (it's a mutable `argv` string).
- This is a **toy cipher**—it's easily broken by frequency analysis.
- Real encryption uses AES, ChaCha20, or similar—not Caesar.

## Why This Matters for Cybersecurity

- The Caesar cipher is the **hello world** of cryptography.
- It teaches **modular arithmetic** and **substitution**.
- Breaking it introduces **frequency analysis**—a core skill.
- It's the first step toward understanding modern ciphers.

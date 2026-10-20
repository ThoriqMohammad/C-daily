# Day 40 — vigenere_cipher

Encrypts and decrypts messages using the **Vigenère cipher**—a
polyalphabetic substitution that defeated cryptanalysts for
three hundred years.

## How It Works

The Vigenère cipher is the Caesar cipher with a twist: instead
of shifting every letter by the same amount, it shifts each letter
by a different amount taken from a repeating keyword.

    key:        LEMON LEMON LEMON LEMON
    plaintext:  ATTAC KATDA WN
    ciphertext: LXFOP VEFRN HR

Each key letter tells us how far to shift its corresponding
plaintext letter: `L` means shift by 11, `E` means shift by 4,
and so on. When the key runs out, it repeats.

For example, encrypting `"A"` with key `"L"` gives `L` (shift
by 11). Encrypting `"T"` with key `"E"` gives `X` (shift by 4).
The same plaintext letter at different positions in the key
cycle produces different ciphertext letters.

## Why This Matters

The Caesar cipher (Day 07) collapses under frequency analysis:
the letter `E` in plaintext is always `H` in ciphertext, so
counting ciphertext frequencies reveals the plaintext. The
Vigenère cipher changes the mapping on every position, so
frequencies get spread out—`E` might encrypt to `H` once,
`L` the next time, `R` after that.

For three centuries this looked like unbreakable code. It was
finally cracked by **Kasiski** (1863) and **Babbage**, using a
two-step attack:

1. Find the key length by looking for repeated ciphertext
   patterns at regular intervals.
2. Once you know the key length, each "column" of the ciphertext
   is just a Caesar cipher—break each one with frequency analysis.

## Usage

    ./vigenere_cipher encrypt KEY "message"
    ./vigenere_cipher decrypt KEY "ciphertext"

The key must be non-empty and contain only letters. Case is
ignored in the key. Non-letters in the message are preserved
and do **not** consume a position in the key.

## Examples

**Encrypt:**

    $ ./vigenere_cipher encrypt LEMON "ATTACKATDAWN"
    LXFOPVEFRNHR

**Decrypt:**

    $ ./vigenere_cipher decrypt LEMON "LXFOPVEFRNHR"
    ATTACKATDAWN

**Mixed case, punctuation:**

    $ ./vigenere_cipher encrypt LEMON "Hello, World!"
    Lxfop, Vefrn!

**Wrong key (decrypting with a different key):**

    $ ./vigenere_cipher decrypt APPLE "LXFOPVEFRNHR"
    Lqjbjwjwsxvn

## Build

    gcc -Wall -Wextra -std=c99 -o vigenere_cipher vigenere_cipher.c

## Skills Practiced

- **Polyalphabetic substitution** — the conceptual leap past Caesar
- **Modular arithmetic** over a key of arbitrary length
- **Key validation** — non-empty, alphabetic, case-insensitive
- **Non-letter handling** — punctuation preserved, key does not advance
- **In-place string transformation** on a mutable `argv` buffer

## The Math

The Caesar cipher was:

    ciphertext[i] = (plaintext[i] + shift) mod 26

The Vigenère cipher replaces the constant `shift` with a
position-dependent value:

    ciphertext[i] = (plaintext[i] + key[i mod keylen]) mod 26

The modulo on `i` is what makes the key repeat. Decryption is
the same formula with `-key[...]` instead of `+key[...]`.

## Key Validation

The program refuses keys that are empty or contain non-letters:

| Key | Valid? | Why |
|-----|--------|-----|
| `LEMON` | ✅ | all letters |
| `lemon` | ✅ | case is normalised |
| `L3M0N` | ❌ | contains digits |
| `` (empty) | ❌ | no shift values |

This is stricter than the original cipher, which allowed any
keyword, but it prevents an entire class of bugs—silently
skipping characters because the key contained a number.

## Notes

- **Non-letters are preserved.** Spaces, punctuation, and digits
  are copied through unchanged. This is a choice: some variants
  of Vigenère include them in the shift, but treating them as
  pass-through keeps the demonstration clean and makes it
  compatible with any plaintext.
- **The key does not advance on non-letters.** Otherwise
  `"AB CD"` and `"ABCD"` with the same key would produce
  different ciphertext, which is surprising.
- **This is a toy cipher.** Vigenère is trivially broken today
  and appears in puzzle books and teaching material, not in
  real security. Use AES for real encryption.
- **No key scheduling.** The key is repeated literally. Modern
  ciphers expand a short key into a long keystream via a
  key-schedule algorithm (e.g., AES round keys); that's the
  bridge between Vigenère and real block ciphers.

## What You Can Extend

- **Autokey** — start with the keyword, then continue the key
  with the plaintext itself; historically the next improvement
  after Vigenère
- **Full-alphabet shift** — include digits and punctuation in
  the shift range, so `"Hello, World!"` becomes slightly
  different ciphertext
- **Kasiski analysis** — write a companion tool that finds
  repeated bigrams and estimates the key length
- **Frequency attack** — given a ciphertext and a guessed key
  length, break each column as Caesar and recover the plaintext
- **Key randomisation** — pick a random key of a fixed length
  and use it like a one-time pad; note the loss of the pad's
  security once the key is reused

## Why This Matters for Cybersecurity

- **Vigenère is the pivot point** in classical cryptography:
  after it, cryptanalysts realised that no fixed-key cipher
  is secure, and that key material must be as long as the
  message (one-time pad) or derived by an algorithm (modern
  ciphers).
- **Kasiski's attack** is the first formal cryptanalysis
  technique based on statistics rather than brute force—an
  early form of the reasoning behind differential cryptanalysis.
- **Polyalphabetic thinking** is the conceptual seed of every
  modern cipher, from AES's substitution-permutation network
  to ChaCha20's keystream.
- The **"key length matters"** lesson from Vigenère generalises
  directly to modern key-length debates: shorter keys make
  brute force and statistical attacks easier, whether the cipher
  is Caesar, Vigenère, or RSA.

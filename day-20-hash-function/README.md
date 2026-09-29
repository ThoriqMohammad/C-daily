# Day 20 — hash_function

Demonstrates several classic string hash functions.

## What Is a Hash Function?

A hash function takes input of **any size** and returns a
**fixed-size value** (the hash).

    "hello"  →  0x1A2B3C4D
    "world"  →  0x5E6F7A8B
    "hello!" →  0x9C0D1E2F

**Same input → same hash. Different input → usually different hash.**

## The Four Hash Functions

| Name | Origin | Formula |
|------|--------|---------|
| **djb2** | Dan Bernstein | `hash * 33 + c` |
| **FNV-1a** | Fowler–Noll–Vo | `(hash ^ c) * prime` |
| **sdbm** | Berkeley DB | `c + (hash << 6) + (hash << 16) - hash` |
| **simple_sum** | Textbook | `sum += c` |

## Usage

    ./hash_function <string>

## Example

    $ ./hash_function hello
    ====================================
            HASH FUNCTION DEMO
    ====================================

    Input: "hello"
    Length: 5

    djb2:       210714636441  (0x310F5A99)
    FNV-1a:     1335831723  (0x4F9F2CAB)
    sdbm:       1752910675  (0x6878C8D3)
    simple_sum: 532  (0x00000214)

## Build

    gcc -Wall -Wextra -std=c99 -o hash_function hash_function.c

## Skills Practiced

- **Bitwise operations** — shifts, XOR
- **Unsigned integer overflow** — defined behavior
- **Modular arithmetic** — hashing math
- **`argc` / `argv`** — input
- **Hashing concepts** — distribution, collisions

## Why Hash Functions Matter

| Domain | Use |
|--------|-----|
| **Hash tables** | Fast lookup |
| **Checksums** | Detect accidental corruption |
| **Cryptographic hashes** | SHA-256, etc. |
| **Password storage** | bcrypt, Argon2 |
| **Data structures** | Bloom filters, sets |

## Properties of a Good Hash

| Property | Meaning |
|----------|---------|
| **Deterministic** | Same input → same hash |
| **Fast** | O(n) for input length n |
| **Uniform** | Outputs evenly distributed |
| **Avalanche** | Small input change → big output change |

## What These Hashes Are NOT

These are **non-cryptographic** hashes.

| They Are Good For | They Are NOT Good For |
|-------------------|----------------------|
| Hash tables | Password storage |
| Caches | Digital signatures |
| Checksums | Security-critical integrity |

**For security, use SHA-256, BLAKE3, or similar.**

## The Trick: 32-bit Overflow

All these functions use `uint32_t`. When the result exceeds
4,294,967,295, it **wraps around**—which is **defined behavior**
for unsigned integers.

This is intentional. The wrapping is part of the hash.

## Notes

- `unsigned char` is used to avoid sign-extension issues.
- The `%u` format prints unsigned integers.
- `%08X` prints hex, zero-padded to 8 digits.
- All four hashes are deterministic and fast.

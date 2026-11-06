# Day 57 — sha256

**SHA-256**: the cryptographic hash function behind TLS, Git,
Bitcoin, and every secure system that needs a fingerprint of
a chunk of data.

## What Is a Hash Function?

A hash function takes any input — a byte, a book, a gigabyte
of video — and produces a fixed-size output. SHA-256 always
produces 32 bytes (256 bits), no matter how big the input is:

    SHA256("")    = e3b0c44298fc1c149afbf4c8996fb924...
    SHA256("abc") = ba7816bf8f01cfea414140de5dae2223...
    SHA256(1 GB file) = 32 bytes

Three properties make a hash function **cryptographically secure**:

1. **Determinism.** Same input, same output, always.
2. **Pre-image resistance.** Given `H = SHA256(x)`, it's infeasible
   to find any `x` from `H`. One-way function.
3. **Collision resistance.** It's infeasible to find any two
   inputs `x`, `y` with `SHA256(x) = SHA256(y)`.

A fourth property — **avalanche effect** — is critical in practice:
changing one bit of input changes ~50% of the output bits. This
means the output looks random even if the input is structured.

## The Algorithm

SHA-256 processes the input in 512-bit (64-byte) blocks. Each
block goes through 64 rounds of mixing, and the output of each
block feeds into the next.

Structure:

1. **Pad the message** to a multiple of 64 bytes.
   - Append a `0x80` byte.
   - Append zeros until length is 56 mod 64.
   - Append the original length as a 64-bit big-endian integer.

2. **Initialize** 8 state words `h0..h7` from constants
   (the fractional parts of the square roots of the first
   8 primes).

3. **For each 64-byte block:**
   - Load the block into 16 32-bit big-endian words.
   - Extend to 64 words via `sigma0`, `sigma1`.
   - Run 64 rounds of the compression function.
   - Add the round output into `h0..h7`.

4. **Output** `h0..h7` as 32 bytes, big-endian.

The 64 rounds are the heart of SHA-256. Each round:

    T1 = h + SIGMA1(e) + CH(e,f,g) + K[i] + w[i]
    T2 = SIGMA0(a) + MAJ(a,b,c)
    h = g; g = f; f = e; e = d + T1
    d = c; c = b; b = a; a = T1 + T2

Where:
- `CH`, `MAJ`, `SIGMA0`, `SIGMA1` are bitwise mixing functions
  using AND, XOR, and right-rotation
- `K[i]` are 64 round constants (cube roots of primes)
- `w[i]` are the expanded message words

The rotation constants (`rotr(x, 2)`, `rotr(x, 13)`, etc.) are
what make SHA-256 hard to invert. Each round shuffles bits in a
different pattern, and 64 rounds is enough to spread any change
across the entire 256-bit state.

## Usage

    ./sha256 <file>       hash a file
    ./sha256 -            hash stdin
    ./sha256 test         run self-tests

## Example

    $ echo -n "abc" | ./sha256 -
    ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad

    $ ./sha256 /etc/passwd
    4f5c3b7d6a1d...
    (32-byte hash of the file)

## Build

    gcc -Wall -Wextra -std=c99 -o sha256 sha256.c

## Skills Practiced

- **Bitwise operations at scale** — AND, OR, XOR, NOT used in
  every round
- **Right-rotate** (`rotr`) — circular shift, not `>>`
- **Message padding** — append length encoding correctly
- **Big-endian byte order** — load words from bytes, emit words
  to bytes
- **Streaming state** — process data in any chunk sizes
- **Test vectors** — verify against published constants
- **Avalanche effect** — a measurable property of a good hash

## Test Vectors

The tests verify against known SHA-256 outputs from the FIPS
180-4 standard. If any test fails, the implementation has a bug.

| Input | Expected SHA-256 (first 32 hex chars) |
|-------|---------------------------------------|
| `""` | `e3b0c44298fc1c149afbf4c8996fb924...` |
| `"abc"` | `ba7816bf8f01cfea414140de5dae2223...` |
| Long pangram | `d7a8fbb307d7809469ca9abcb0082e4f...` |

You can verify any hash independently with:

    $ echo -n "abc" | sha256sum
    ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  -

The SHA-256 implementation in this file should match `sha256sum`
exactly. If it doesn't, one of the test vectors will fail.

## The Padding Scheme

Padding is subtle and easy to get wrong. The rule:

1. Append a single byte `0x80`.
2. Append zero bytes until the total length is **56 mod 64**.
   (That's 56 bytes into the last 64-byte block, leaving 8
   bytes for the length.)
3. Append the **original message length in bits** as a 64-bit
   big-endian integer.

For an empty message:

    Total length: 0 bits
    After padding: 0x80, then 55 zeros, then 8 bytes of 0
    Total: 64 bytes

For a 3-byte message:

    Total length: 24 bits
    After padding: "abc", then 0x80, then 52 zeros, then
                   0x00..0x18 (24 in big-endian)
    Total: 64 bytes

The length field encodes **bits**, not bytes. `len * 8` is what
gets appended. Getting this wrong breaks everything silently —
the hash is still 32 bytes, but it's the wrong 32 bytes.

## The Avalanche Effect

Test 2 shows this concretely:

    SHA256("abc") = ba7816bf8f01cfea...
    SHA256("abd") = 3f1d94c1d5e1d19f...  (40-50% of bits differ)

A single byte change should change ~50% of output bits. If a
hash function has this property, it's called a **hash with
avalanche effect**. MD5 fails this (in theory, in practice it's
broken); SHA-1 fails this too; SHA-256 has it.

## Why SHA-256 and Not MD5 or SHA-1?

| Algorithm | Output | Status |
|-----------|--------|--------|
| MD5 | 128 bits | Broken (collisions found 2004) |
| SHA-1 | 160 bits | Broken (SHAttered attack, 2017) |
| SHA-256 | 256 bits | Secure |
| SHA-3 | 256 bits | Secure (different design) |

MD5 and SHA-1 are still used in practice where collisions don't
matter (e.g., file checksums), but they should never be used for
security. SHA-256 has no known practical attacks.

**The SHAttered attack (2017)** demonstrated a SHA-1 collision
in practice: two different PDF files with the same SHA-1 hash.
This broke SHA-1 for all security purposes and prompted a
migration to SHA-256 that's still in progress today.

## Where SHA-256 Is Used

- **TLS certificates** — signing, integrity verification
- **SSH** — key fingerprints, signatures
- **Git** — commit hashes (SHA-1 historically, SHA-256 available)
- **Bitcoin** — block hashes, mining, address derivation (double SHA-256)
- **Password hashing** — as a component of PBKDF2, bcrypt, scrypt, Argon2
- **HMAC** — message authentication (SHA-256 is HMAC's default hash)
- **File integrity** — checksumming downloads, content-addressed storage (Git, IPFS)
- **Digital signatures** — RSA-PSS, ECDSA both use SHA-256

## Streaming vs One-Shot

The `sha256_update` function can be called multiple times with
chunks of data. Test 3 verifies that feeding a message
byte-by-byte produces the same hash as hashing it in one call:

    sha256_update(ctx, "a", 1);
    sha256_update(ctx, "b", 1);
    sha256_update(ctx, "c", 1);
    /* equals */
    sha256_update(ctx, "abc", 3);

This is why the streaming API exists — real files are too large
to fit in memory, so you hash in 4KB or 64KB chunks.

## Notes

- **`rotr(x, n)` is not `x >> n`.** The shift loses bits; the
  rotation brings them back at the top. Use `(x >> n) | (x << (32-n))`.
  The bug of using `>>` instead of `rotr` produces a hash that
  looks plausible but fails all test vectors.
- **Big-endian everywhere.** SHA-256 specifies big-endian for
  both the input block loading and the output byte emission.
  Little-endian will produce a different hash.
- **The length field is 64 bits.** Even on 32-bit platforms,
  the length is 64 bits. For files > 2^61 bytes, you'd need
  to handle overflow, but that's beyond the horizon.
- **Constants come from constants.** `K[]` and `H0[]` are
  derived from cube roots and square roots of primes. They
  look random but aren't. Using different constants would
  produce a different hash function — but not SHA-256.
- **This is not the fastest implementation.** It processes 64
  bytes at a time without SIMD. Real crypto libraries use
  hardware SHA extensions (Intel SHA-NI, ARM crypto extensions)
  that are 10-50× faster.
- **Not constant-time.** SHA-256 itself is generally
  data-independent, but the load/store patterns in this
  implementation could leak information through cache timing.
  For constant-time code (e.g., in side-channel-critical
  applications), use a library.

## What You Can Extend

- **HMAC-SHA256** — message authentication (SHA-256 + key)
- **PBKDF2** — password hashing using SHA-256 with iterations
- **SHA-512** — the sibling algorithm with 64-bit words
- **SHA-3** — a completely different construction (Keccak sponge)
- **SIMD acceleration** — use Intel SHA-NI to hash 10× faster
- **File hashing with size** — output `<hash>  <filename>` like `sha256sum`
- **Compare with `sha256sum`** — verify byte-for-byte
- **A content-addressed store** — name files by their hash

## Why This Matters for Cybersecurity

- **SHA-256 is the workhorse of crypto.** Every TLS connection,
  every Git commit, every Bitcoin block, every SSH key
  fingerprint — all use SHA-256.
- **Collision resistance is the critical property.** It's what
  prevents an attacker from substituting one document for
  another with the same signature, or one file for another
  with the same checksum.
- **The SHAttered attack (2017) was a watershed moment.** It
  broke SHA-1 in practice, not just in theory, and forced
  the world to migrate to SHA-256 over the following years.
- **Understanding hash functions is prerequisite to understanding**:
  - Digital signatures (you sign the hash, not the message)
  - HMAC (authentication on top of hashing)
  - Password storage (salt + slow hash)
  - Merkle trees (hash trees for integrity)
  - Proof-of-work (Bitcoin mining)
- **The implementation is short enough to read.** SHA-256 is
  ~150 lines of C. Unlike RSA or elliptic curves, it's
  completely tractable to read, understand, and modify.

## Why This Matters as a Portfolio Piece

- **SHA-256 is a recognizable achievement.** Anyone in security
  knows what it is and will appreciate seeing it implemented.
- **The test vectors are verifiable.** You can compute
  `SHA256("abc")` in any terminal with `sha256sum` and compare
  to the output of your program. That's proof-of-work.
- **The algorithm is well-specified.** Unlike "write a service"
  projects, there's a definitive right answer. A correctly
  implemented SHA-256 that passes all standard test vectors is
  provably correct, not just "looks good."
- **It completes the hash story** started on Day 20
  (`hash_function`, a simplified teaching hash). Together they
  show the spectrum from "toy hash" to "real cryptographic hash."

## The Crypto Arc

| Day | Project | Role |
|-----|---------|------|
| 07 | Caesar cipher | Historical starting point |
| 11 | XOR cipher | Stream cipher basics |
| 20 | Hash function | Toy hash, teaching |
| 25 | Base64 | Encoding (not crypto) |
| 32 | Password manager | Applied password storage |
| 40 | Vigenère cipher | Polyalphabetic |
| 45 | Modular exponentiation | Public-key primitive |
| 48 | Prime factorization | RSA's hard problem |
| 50 | RSA | Public-key encryption |
| 55 | Diffie-Hellman | Public-key key exchange |
| **57** | **SHA-256** | **Cryptographic hash function** |

That's a complete cryptography curriculum: ciphers, hashes,
public-key encryption, key exchange, and the primitives that
underpin all of them.

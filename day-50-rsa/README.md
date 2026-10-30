# Day 50 — rsa

A working **RSA** public-key cryptosystem in C. This is the
capstone of the whole run: it ties together arbitrary-precision
arithmetic (Day 43), modular exponentiation (Day 45), and prime
factorization (Day 48) into a real encryption scheme.

## What Is RSA?

RSA (Rivest–Shamir–Adleman, 1977) was the first public-key
cryptosystem. It allows two parties who have never met to
communicate securely over an open channel:

1. Alice publishes a **public key**.
2. Bob encrypts a message with the public key and sends it.
3. Only Alice (who holds the **private key**) can decrypt.

The security rests on a single asymmetry:

> Multiplying two large primes is easy.
> Factoring their product back is hard.

For 2048-bit RSA, generating the key takes milliseconds.
Factoring the resulting 2048-bit number would take longer than
the age of the universe, using any known classical computer.

## How It Works

**Key generation:**

1. Pick two large random primes `p` and `q`.
2. Compute `n = p × q` (the public modulus).
3. Compute `φ(n) = (p-1) × (q-1)` (Euler's totient).
4. Choose `e` coprime to `φ(n)` (usually 65537).
5. Compute `d = e^(-1) mod φ(n)` via the extended Euclidean algorithm.

**Public key** = `(n, e)` — can be shared with anyone.
**Private key** = `(n, d)` — kept secret.

**Encryption** of message `m` (with `m < n`):

    c = m^e mod n

**Decryption** of ciphertext `c`:

    m = c^d mod n

Both operations use modular exponentiation (Day 45), and both
work for numbers up to 2048 bits — which requires big-integer
arithmetic (Day 43) for real keys.

## A Tiny Worked Example

The example used throughout the tests:

    p = 61
    q = 53
    n = 61 * 53 = 3233
    φ(n) = 60 * 52 = 3120
    e = 17    (chosen because gcd(17, 3120) = 1)
    d = 17^(-1) mod 3120 = 2753

Encrypt `'A'` (ASCII 65):

    c = 65^17 mod 3233 = 2790

Decrypt:

    m = 2790^2753 mod 3233 = 65

That's the entire cipher.

## Why `d` Works

You might wonder why encrypting with `e` and decrypting with `d`
recovers the message. The reason is Euler's theorem: for any `a`
coprime to `n`,

    a^φ(n) ≡ 1 (mod n)

And `e × d ≡ 1 (mod φ(n))`, so `e × d = 1 + k × φ(n)` for some `k`.

Therefore:

    (m^e)^d = m^(e×d) = m^(1 + k×φ(n)) = m × (m^φ(n))^k ≡ m × 1^k = m (mod n)

That is the entire security proof.

## Why `n` Cannot Be Factored

Attacker sees `n = 3233` and the public exponent `e = 17`. To
decrypt, they need `d = e^(-1) mod φ(n)`. That requires knowing
`φ(n) = (p-1)(q-1)`. That requires factoring `n` back into `p × q`.

For `n = 3233`, factoring takes microseconds (trial division to
~56). For a 2048-bit `n`, it would take longer than the universe
has existed. That's the security.

## Usage

    ./rsa

Runs four tests and prints `ok` or `FAIL` for each assertion.

## Build

    gcc -Wall -Wextra -std=c99 -o rsa rsa.c

## Skills Practiced

- **RSA key generation** with Miller-Rabin primality testing
- **Modular inverse** via the extended Euclidean algorithm
- **Modular exponentiation** (reused from Day 45)
- **Euler's totient** function
- **Public/private key separation**
- **Byte-to-integer encoding** for encryption
- **Why textbook RSA is insecure**

## Real vs Toy RSA

| Aspect | This demo | Production |
|--------|-----------|------------|
| Prime bits | ~16 | 1024+ |
| n bits | ~32 | 2048+ |
| Block size | 1 byte | ~190 bytes |
| Big integers | `unsigned long long` | GMP, BIGNUM |
| Padding | none | OAEP or PKCS#1 v1.5 |
| Speed | microseconds | milliseconds |
| Security | broken in seconds | computationally infeasible |

The math is **identical** at every step. The difference is
that production RSA uses numbers with hundreds of digits, which
requires a big-integer library to manipulate.

## Why Textbook RSA Is Insecure

The code in this file is "textbook RSA": the naive mathematical
implementation. It's correct but insecure for three reasons:

1. **Deterministic.** Encrypting `65` always gives the same
   ciphertext. An attacker can encrypt guesses and compare, so
   even without the key, they can test candidate plaintexts.

2. **Malleable.** If `c = m^e mod n`, then `c × k^e mod n = (m×k)^e
   mod n`. An attacker can transform ciphertexts into ciphertexts
   of related plaintexts without decrypting anything.

3. **Small-message leaks.** If `m` is small enough that `m^e < n`,
   then `c = m^e` exactly (no modulus applied), so `m = e-th root of c`.
   This leaks `m` for small messages.

Real RSA adds **padding** — random data mixed with the message
before encryption — to defeat all three. **OAEP** (Optimal
Asymmetric Encryption Padding) is the standard choice.

`test_why_padding_matters` demonstrates the determinism issue
directly: encrypting the same byte twice gives identical results.

## Real RSA in Use

Every HTTPS connection you make uses RSA (or a similar
algorithm) for at least part of the handshake. The pattern:

1. The server has a **certificate** containing its public key.
2. The client verifies the certificate against a trusted CA.
3. The client encrypts a **session key** with the server's
   public key.
4. The session key is used with AES for the actual data.

RSA is used only for the **key exchange** — the encryption of
a small random value. It's far too slow to encrypt bulk data
directly. AES does that.

## Notes

- **Primes here are tiny** (~16 bits), so `n` fits in 32 bits.
  A real RSA-2048 key has primes of ~1024 bits each, requiring
  bignum arithmetic.
- **The `random_prime` function is simple.** For cryptographic
  use, you need:
  - A cryptographically secure PRNG (`getrandom` on Linux, not
    `rand()`)
  - Primality testing on 1024-bit candidates (Miller-Rabin with
    many rounds, since 12 bases aren't enough)
  - A check that `p - 1` and `q - 1` have large prime factors
    (to avoid factorization via `p-1` algorithms)
- **`e = 65537` is standard.** It's `F4`, the fourth Fermat
  number. It's prime, it's small enough for fast encryption,
  and it's large enough that `m^e` overflows typically (making
  the small-message attack harder).
- **The program only encrypts single bytes.** Real RSA encrypts
  blocks of up to `k - 11` bytes where `k` is the byte length
  of `n`, minus padding overhead. This demo encrypts one byte
  at a time to stay within the range of `u64`.
- **`test_why_padding_matters`** is not a bug fix — it's a
  demonstration. The deterministic behavior is *correct for
  textbook RSA* and *insecure for real use*. That's the lesson.

## What You Can Extend

- **OAEP padding** — the standard fix for textbook RSA
  insecurities
- **Big-integer arithmetic** — use Day 43's `big_integer` to
  handle 2048-bit keys
- **`__int128` moduli** — support up to 64-bit `n` with
  128-bit intermediates
- **Blinding** — randomize decryption to prevent timing
  side-channels (RSA blinding)
- **Digital signatures** — the same math in reverse:
  sign with `d`, verify with `e`
- **A real file encryptor** — read a file, encrypt in blocks,
  write ciphertext
- **Compare with OpenSSL** — verify your implementation
  produces the same ciphertext for the same key

## Why This Matters for Cybersecurity

- **RSA is everywhere.** TLS certificates, SSH keys, PGP email,
  code signing, JWT signatures, S/MIME — all use RSA in some
  form.
- **The asymmetry is the basis of the internet.** Without
  public-key cryptography, every encrypted connection would
  require a pre-shared secret, which doesn't scale.
- **Implementation matters.** The math is well understood, but
  bugs in padding, RNG, or blinding have broken real systems:
  - **Bleichenbacher's attack (1998)** — padding oracle in
    PKCS#1 v1.5
  - **ROBOT attack (2017)** — reappearance of Bleichenbacher
    in real TLS libraries
  - **Debian OpenSSL bug (2008)** — a bad RNG made every
    Debian-generated RSA key factorable
  - **Many more** — the gap between "correct math" and
    "secure implementation" is where real attacks live
- **Post-quantum threat.** Shor's algorithm on a large quantum
  computer would break RSA entirely. The migration to
  post-quantum algorithms (ML-KEM, ML-DSA) is happening now.
  Understanding RSA is prerequisite to understanding what
  replaces it.

## Why This Matters as a Portfolio Piece

- **RSA is the "hello world" of public-key cryptography.** Any
  security role will expect you to be able to explain it, and
  having written it is much more convincing than having read
  about it.
- **The dependency chain is visible.** You can point to Day 43
  (`big_integer`), Day 45 (`modular_exponentiation`), Day 48
  (`prime_factorization`), and Day 50 (`rsa`) as a coherent
  progression.
- **The gap between "textbook" and "secure" is the whole job.**
  Being able to articulate *why* this code is insecure — and
  what the fix is — signals the kind of judgment that matters
  in security engineering.
- **It closes the loop on the "why" of the earlier crypto
  days.** Caesar, XOR, Vigenère, hashing, modular
  exponentiation — each was a step toward this. Now it's
  complete.

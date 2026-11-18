# Day 69 — padding_oracle_demo

A demonstration of **Bleichenbacher's padding oracle attack**
on RSA with PKCS#1 v1.5 padding. The attack works because a
server leaks one bit of information through its error
responses.

This is not a new attack — it was described in 1998 — but it
has resurfaced repeatedly because implementations keep getting
the defense wrong. The 2017 ROBOT attack found it in
production TLS libraries including OpenSSL, BoringSSL, NSS,
and more.

The lesson is not "RSA is broken." The lesson is **the math is
easy; the implementation is hard**.

## The Setup

RSA with PKCS#1 v1.5 padding:

1. Build the padded block: `0x00 0x02 [PS] 0x00 [M]`
   - `PS` is 8+ random non-zero bytes
   - `M` is the message (up to k-11 bytes)
2. Interpret the block as a big-endian integer.
3. Encrypt: `c = m^e mod n`.
4. Decrypt: `m = c^d mod n`.
5. Check padding: does the result start with `0x00 0x02`?

Step 5 is where the attack lives.

## The Oracle

A **padding oracle** is any system that tells the attacker
whether the padding was valid or not. It can be:

- A server that returns "decryption failed" vs "invalid request"
- A server that returns different status codes
- A server that takes different amounts of time
- A server that closes the connection vs. keeps it open

If the attacker can distinguish these cases, they have an oracle.

## The Attack

Bleichenbacher's attack recovers the plaintext using only the
oracle, without ever seeing the private key. Simplified:

1. Choose a random integer `s` (the "multiplier").
2. Compute `c' = c * s^e mod n`. This corresponds to a
   modified plaintext `m' = m * s mod n`.
3. Ask the oracle about `c'`.
4. The answer tells you whether `m'` has valid padding.
5. Repeat with different `s` values, maintaining a **range**
   `[a, b]` of possible `m` values.
6. Each oracle response halves the range.
7. After enough queries, the range collapses to a single value
   — the plaintext.

For a 1024-bit RSA key, the attack needs about **2^20 queries**
(one million). That's seconds to minutes against a network
oracle, with no brute-forcing.

For the toy 30-bit key in this file, the "attack" is more
about demonstrating the oracle leak than actually recovering
the plaintext, because the toy key is too small to be
meaningful. The **concept** — one bit of leakage per query —
is what matters.

## The Demo

The program runs three demos:

1. **RSA with PKCS#1 v1.5** — pad, encrypt, decrypt, unpad.
   Shows the mechanics and confirms the round-trip works.

2. **The oracle leak** — shows that the oracle answers
   differently for valid vs. invalid padding. Runs 1000 random
   ciphertexts and counts how many "accidentally" have valid
   padding. The answer is nonzero, which is all an attacker
   needs.

3. **Constant-time defense** — shows a constant-time padding
   check that (in principle) eliminates the timing leak.

## The Critical Observation

Notice the asymmetry: the attacker never needs to *decrypt*
anything. They only need to know "did this ciphertext have
valid padding?" That's one bit. With enough of those bits,
they reconstruct the plaintext.

This is the essence of a **side-channel attack**: information
leaks not through the intended interface, but through
observable consequences of the implementation.

The math of RSA is provably secure under standard
assumptions. The **system** is only as secure as the
weakest observable signal.

## Why PKCS#1 v1.5 Padding and Not OAEP

PKCS#1 v1.5 (the 1991 scheme) is the one Bleichenbacher
attacked. It's still common in legacy systems, but modern
RSA encryption uses **OAEP** (Optimal Asymmetric Encryption
Padding, 1994), which is designed to resist this attack.

The difference: with OAEP, the "valid padding" event is so
rare (essentially a hash collision) that the oracle isn't
useful. Bleichenbacher's attack doesn't work against OAEP.

This is why the TLS 1.3 spec **bans PKCS#1 v1.5 for key
exchange** and allows only OAEP or ECDH. TLS 1.2 allowed
v1.5 and was the target of the ROBOT attack.

## The Defense

The demo shows a `pkcs1_check_constant_time` function. The
goal: no branches that depend on the padding bytes, so
the timing doesn't leak whether padding was valid.

**This defense is subtle and easy to get wrong.** The
version in this file is illustrative, not production-quality.
A truly correct constant-time check:

- Uses only bitwise operations (no `if`, no early return)
- Accumulates a result mask
- Returns at the end, unconditionally

Even a correct constant-time check is only *part* of the
defense. The complete recipe (per the Bleichenbacher
mitigations):

1. **Constant-time padding check** — no timing leak from the
   check itself
2. **Constant-time error response** — same HTTP status, same
   body length, same response time regardless of failure mode
3. **Randomized response delay** — adds noise so timing is
   less informative
4. **Session termination on failure** — close the connection
   so the attacker can't keep querying
5. **Logging and rate limiting** — detect unusual query
   patterns and throttle
6. **Use OAEP instead of v1.5** — the real fix

Every one of these is required. Missing any one leaves the
door open.

## Usage

    ./padding_oracle_demo

Runs all three demos and prints the results.

## Build

    gcc -Wall -Wextra -std=c99 -o padding_oracle_demo padding_oracle_demo.c

## Skills Practiced

- **PKCS#1 v1.5 padding** — the actual bit layout
- **RSA encryption/decryption** — reused from Day 50
- **The oracle model** — any observable signal is a side
  channel
- **Side-channel thinking** — "what does the implementation
  leak that the algorithm doesn't?"
- **Constant-time code** — the principle, not just the syntax
- **Historical context** — Bleichenbacher 1998, ROBOT 2017
- **Defense in depth** — why one mitigation isn't enough

## The Bleichenbacher Family Tree

This attack spawned a whole family of related attacks:

| Attack | Year | Target |
|--------|------|--------|
| Bleichenbacher's original | 1998 | PKCS#1 v1.5 in TLS |
| Klima-Pokorny-Rosa | 2003 | OpenSSL timing |
| Meyer et al. | 2014 | XML encryption |
| "DROWN" | 2016 | TLS v1.0 |
| "ROBOT" | 2017 | TLS libs still vulnerable |
| "Marvin" | 2023 | Various (new variants) |

Every few years, someone finds that a production system is
still vulnerable to a variant. The attack is *that* hard to
defend against correctly.

## Notes

- **The toy key is 30 bits.** Real RSA is 2048+. The
  attack works at every size; only the query count changes.
  This file uses a small key so the demo runs instantly.
- **The attack isn't fully implemented.** Implementing
  Bleichenbacher's interval-search version is a substantial
  project (the original paper is ~20 pages of math). This
  file demonstrates the *oracle leak*, which is the
  critical primitive. A full attack implementation is left
  as an exercise.
- **The "leak count" is illustrative.** Running 1000 random
  ciphertexts through the oracle shows some are accepted
  as valid padding. In real RSA (2048-bit), about 1 in
  2^16 random ciphertexts have valid padding. The attacker
  exploits that nonzero rate.
- **The constant-time check is illustrative, not production.**
  A production version would be written with bitwise
  masking, tested for timing independence, and audited for
  compiler-introduced branches. This one demonstrates the
  principle.
- **The `ps_count >= 8` check is not constant-time in this
  code.** A production version would compute
  `(ps_count - 8) >> 63` (for signed arithmetic) to get a
  0/1 without branching.
- **The `s` (multiplier) in the attack is not implemented.**
  In a real attack, `s` is chosen to narrow the plaintext
  interval. This demo shows the *concept* of the oracle;
  implementing the interval search is the hard part.
- **The padding is deterministic.** Real PKCS#1 v1.5 uses
  random `PS`. This demo uses a fixed pattern so the
  output is reproducible. Real code must use a
  cryptographically secure random source.

## What You Can Extend

- **Full Bleichenbacher attack** — implement the interval
  search. This is genuinely hard (~100 lines of careful
  math) and the payoff is a working attack against the
  toy RSA.
- **OAEP padding** — implement PKCS#1 v2.x and show that the
  oracle becomes useless. That's the modern defense.
- **Timing side-channel measurement** — actually measure
  `pkcs1_check` execution time with different inputs and
  show that the "constant-time" version has more uniform
  timing than the naive version.
- **A real HTTPS server with a v1.5 oracle** — wrap an
  HTTP server around the toy RSA decryption, demonstrate
  the attack against it, then fix it with OAEP.
- **Compare with `openssl s_client`** — set up a test TLS
  server with v1.5 and try a timing attack. Real tools can
  detect the oracle.

## Why This Matters for Cybersecurity

- **The "leak one bit" pattern is universal.** Timing
  attacks on AES (Bernstein 2005), cache attacks
  (Spectre, Meltdown), power analysis, acoustic analysis
  — all exploit side channels. Bleichenbacher is the
  textbook example.
- **"Correct math" ≠ "secure implementation."** RSA math
  is provably secure. The implementation is where the
  attack lives. This distinction is *the* core lesson of
  applied cryptography.
- **Every "invalid input" error is a potential oracle.**
  The error message, status code, response length, and
  response time all leak information. Designing secure
  error handling is a real skill.
- **The ROBOT attack (2017)** showed that a 20-year-old
  attack was still exploitable in production. If you think
  "this vulnerability is old, so nobody is vulnerable
  anymore," the history of Bleichenbacher says otherwise.
- **Defense in depth is essential.** No single mitigation
  is enough. Constant-time code + uniform error responses
  + randomized delays + session termination + rate
  limiting + OAEP. Every one matters.

## Why This Matters as a Portfolio Piece

- **"I understand Bleichenbacher"** is a strong signal.
  It's the kind of attack that appears in interviews for
  security engineering roles, and being able to explain
  it — let alone implement it — is impressive.
- **The code is small** — the oracle is one function, the
  padding check is another, and the demo connects them.
  This is the kind of "small, sharp tool" that a
  portfolio should have.
- **The historical context** (1998, 2017, 2023) shows you
  follow the field, not just the textbook.
- **The defense discussion** is critical. Security
  engineers need to know not just how to attack, but how
  to defend. The "Defense in depth" section is as
  important as the attack.
- **It connects to Day 50 (`rsa`).** The two entries form
  a pair: "here's how RSA works" (Day 50) and "here's how
  it's broken" (Day 69). A reviewer sees both and knows
  you understand RSA at the implementation level.

## The Crypto Arc

| Day | Project | Role |
|-----|---------|------|
| 07 | `caesar_cipher` | Classical cipher |
| 11 | `xor_cipher` | Stream cipher |
| 20 | `hash_function` | Basic hash |
| 25 | `base64_encoder` | Encoding |
| 32 | `password_manager` | Applied hashing |
| 40 | `vigenere_cipher` | Polyalphabetic |
| 45 | `modular_exponentiation` | Public-key primitive |
| 48 | `prime_factorization` | RSA's hard problem |
| 50 | `rsa` | Public-key encryption |
| 55 | `diffie_hellman` | Key exchange |
| 57 | `sha256` | Cryptographic hash |
| 60 | `hmac` | Keyed hash |
| **69** | **`padding_oracle_demo`** | **Real attack on RSA** |

This is the entry that transforms the crypto collection from
"here are algorithms" to "here's how attacks actually work."
That's the difference between a textbook and a working
security engineer.

## A Note on Ethical Use

Implementing an attack is educational. Running it against
systems you don't own is illegal. The toy RSA in this file
is designed to be attacked — that's the whole point. Don't
try the same thing against a real production server. The
techniques demonstrated here are only for understanding,
not for unauthorized access.

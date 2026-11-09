# Day 60 — hmac

**HMAC** (Hash-based Message Authentication Code): a keyed hash
that proves both the authenticity and the integrity of a message.

## What HMAC Does

HMAC is built from any hash function. With SHA-256, it produces
a 32-byte tag that:

- Anyone with the **same key** can recompute
- No one **without the key** can forge
- Any change to the message invalidates the tag

That combination is exactly what you need for:

- **Message authentication** — "this message really came from
  someone holding the key"
- **Integrity** — "no one tampered with this message in transit"

## Why Not Just Hash the Key Into the Message?

The naive options all have problems:

    H(key || message)          <- length-extension attack
    H(message || key)          <- vulnerable to
                                  chosen-prefix attacks
    H(key || message || key)   <- "sandwich", still not secure

**Length-extension attack** is the main one. SHA-256 (and
SHA-1, MD5, and other Merkle-Damgård hashes) leak internal
state: given `H(prefix)` and the length of `prefix`, an attacker
can compute `H(prefix || suffix)` for any suffix they choose,
**without knowing prefix**. If prefix is a secret key, this
lets them forge MACs.

That's why HMAC uses a different structure — it doesn't rely on
the hash being a "strong" mode of operation; it uses the hash
as a black box in a specific two-layer construction.

## The HMAC Construction

HMAC (RFC 2104) is:

    HMAC(K, m) = H((K ^ opad) || H((K ^ ipad) || m))

Where:
- `K` is the key, padded to the hash's block size (64 bytes for SHA-256)
- If `len(K) > block_size`, `K` is replaced by `H(K)`
- `ipad` is the byte `0x36` repeated block_size times
- `opad` is the byte `0x5c` repeated block_size times
- `||` is concatenation

In code:

```c
uint8_t k_block[64] = {0};
if (key_len > 64) {
    sha256(key, key_len, k_block);   /* hash to 32 bytes */
} else {
    memcpy(k_block, key, key_len);
}

for (i = 0; i < 64; i++) {
    ipad[i] = k_block[i] ^ 0x36;
    opad[i] = k_block[i] ^ 0x5c;
}

uint8_t inner[32];
sha256_concat(ipad, msg, inner);          /* H(ipad || msg) */
sha256_concat(opad, inner, out);          /* H(opad || inner) */

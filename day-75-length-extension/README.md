# Day 75 — length_extension

The **length-extension attack** on Merkle-Damgård hash
functions. Given `H(secret || message)` and
`len(secret || message)`, an attacker can compute
`H(secret || message || pad || suffix)` for any suffix,
**without knowing the secret**.

This is the attack that motivated HMAC (Day 60). It's also
why SHA-3 exists, and why every major web framework has at
some point had a CVE for "we used SHA256(secret||msg) as a
signature and someone forged tokens."

## The Vulnerability

SHA-256 (and SHA-1, MD5, SHA-512, ...) are **Merkle-Damgård**
hash functions. They process input in blocks, maintaining an
internal state that's updated per block. The final digest is
the state after the last block is processed — with a specific
padding appended to the last block.

Two critical facts:

1. **The padding is deterministic given the message length.**
   No secret in the padding. Just `0x80`, zeros, and the
   length in bits as a 64-bit big-endian integer.

2. **The digest is exactly the internal state**, big-endian
   encoded. Nothing else.

Therefore: if you know the digest and the message length,
you know the internal state *after the message and its
padding*, and you know exactly what bytes are between the
message and the digest. So you can continue processing.

## The Attack, in Steps

Setup:
- Server computes `MAC = SHA256(secret || message)` and sends
  both `message` and `MAC` to the client.
- Attacker sees `message` and `MAC`. Attacker does NOT see
  `secret`, but can guess `len(secret || message)` (usually
  deducible from message length plus a typical secret length).

Attack:
1. Compute the padding `P` that the server's SHA-256 applied
   after `secret || message`. `P` depends only on
   `len(secret || message)`, which the attacker knows.
2. Set the SHA-256 internal state to `MAC` (parsed as 8
   big-endian 32-bit words).
3. Set the bit counter to `(len(secret || message) + len(P)) * 8`.
4. Update the hash with `suffix` (attacker-chosen, e.g.
   `"&role=admin"`).
5. Finalize to get `forged_MAC`.

Result: `forged_MAC = SHA256(secret || message || P || suffix)`.

Now the attacker sends:
- `forged_message = message || P || suffix`
- `forged_MAC`

The server computes `SHA256(secret || forged_message)` and
gets exactly `forged_MAC`. The server accepts the forged
request.

That's it. No secret needed. No cryptanalysis. Just
continuing a hash.

## Why It Works: The Merkle-Damgård Picture

SHA-256's structure:

    block 0  →  state_0  →  compress  →  state_1
    block 1            →  compress  →  state_2
    ...
    block n            →  compress  →  state_n  ← THE DIGEST

Each block is XOR-ed into the state, then compressed. The
final digest is `state_n`, which is exactly the internal
state.

If you know `state_n` (the digest), you can start "block n+1"
with `state_n` as your initial state. The hash doesn't care
that you "skipped" blocks 0..n — it just processes whatever
you give it.

This is the entire vulnerability. **The digest contains the
internal state**, and there's nothing binding the message
content to the state other than the fact that blocks 0..n
were processed. An attacker who knows `state_n` can continue.

## Why HMAC Fixes This

HMAC-SHA256 (Day 60) is:

    HMAC(secret, message) = SHA256(secret ^ opad ||
                                   SHA256(secret ^ ipad || message))

Two hashes, not one. The outer hash's input is
`secret ^ opad || inner_hash`, and the inner hash's input
is `secret ^ ipad || message`. Neither is "secret || message."

To apply length extension to HMAC, the attacker would need to
know the internal state *after* `secret ^ ipad || message` is
processed — i.e., the inner hash before finalization. But the
client only sees the **outer** hash's output, which is
`SHA256(secret ^ opad || inner_hash)`. Length extension from
*that* is possible in principle, but the resulting "message"
would be `inner_hash || pad || attacker_suffix` — which doesn't
correspond to any controllable input to the inner hash. The
attacker can't extend the inner hash, so they can't forge.

HMAC was published (1996) as a response to this kind of
attack. Its "secret in both hash inputs" structure is the
fix.

## The Demo

`./length_extension demo` walks through the attack against
a fake server:

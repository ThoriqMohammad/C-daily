# Day 42 — bloom_filter

A **Bloom filter**: a probabilistic set that trades a small
false-positive rate for enormous space savings.

## What Is a Bloom Filter?

A Bloom filter answers one question:

> "Is this item in the set?"

It has two answers:

| Answer | Meaning |
|--------|---------|
| **definitely not present** | always correct |
| **maybe present** | occasionally wrong |

- **False negatives are impossible.** If you inserted the item,
  the filter will always say "maybe present."
- **False positives are possible.** The filter will sometimes
  say "maybe present" for an item that was never added.

That asymmetry is the whole point. The filter is a **cheap
pre-check**: if it says "definitely not present," you know the
item is absent and can skip an expensive lookup. If it says
"maybe present," you do the real lookup to confirm.

## How It Works

The structure is just two things:

1. A **bit array** of `m` bits, initially all zero.
2. `k` **hash functions**, each mapping an item to a bit position.

**Add an item:**

    for k hash functions:
        h = hash(item, k) % m
        bits[h] = 1

**Check for an item:**

    for k hash functions:
        h = hash(item, k) % m
        if bits[h] == 0:
            return "definitely not present"
    return "maybe present"

If any of the `k` bits is zero, the item was never added — because
adding it would have set all `k` bits. If all `k` bits are one,
the item *probably* was added, but the bits could have been set by
other items.

## A Visual Example

With `k = 3` and `m = 16`, adding `"cat"` sets bits 2, 7, 12:

    bits:  0 0 1 0 0 0 0 1 0 0 0 0 1 0 0 0
                    ^           ^     ^

Now checking `"dog"` hashes to bits 2, 7, 9:

    bits:  0 0 1 0 0 0 0 1 0 0 0 0 1 0 0 0
                    ^           ^  ^ (0!)

Bit 9 is zero, so `"dog"` is **definitely not present**.

Checking `"fish"` hashes to bits 2, 7, 12:

    all three bits are 1 → "maybe present"

But we never added `"fish"` — those bits were set by `"cat"`.
This is a **false positive**.

## Usage

    ./bloom_filter

Runs four tests and prints the filter's behaviour.

## Build

    gcc -Wall -Wextra -std=c99 -o bloom_filter bloom_filter.c

## Skills Practiced

- **Bit arrays** — storing `m` bits in `m/8` bytes
- **Bit operations** — `|= 1u << pos` to set, `>> & 1` to check
- **FNV-1a hashing** — simple, fast, well-distributed for short strings
- **Seeded hashing** — using different seeds to derive `k` positions
- **Probability models** — reasoning about false positives
- **Separation of concerns** — bit ops, hashing, and the Bloom API are distinct

## Why Hash Quality Matters

A Bloom filter is only as good as its hash function. A **bad**
hash (one that clusters outputs) would repeatedly map different
items to the same bits, rapidly filling the array and driving
the false-positive rate to 100%. FNV-1a has good avalanche
behaviour — small changes in input produce large changes in
output — which is exactly what a Bloom filter needs.

For real production use, cryptographic hashes (SHA-256, BLAKE3)
or well-tested non-crypto hashes (MurmurHash3, xxHash) are
preferred. FNV-1a is fine for a toy, and it's short enough to
read in a sitting.

## Estimated False-Positive Rate

For `m` bits, `k` hashes, and `n` items, the false-positive
rate is:

    FP ≈ (1 - e^(-kn/m))^k

| `m` (bits) | `k` | `n` (items) | Expected FP rate |
|-----------|-----|-------------|------------------|
| 1024 | 3 | 100 | ~2% |
| 4096 | 4 | 200 | ~0.1% |
| 4096 | 4 | 1000 | ~30% |
| 8192 | 7 | 1000 | ~1% |

Notice what happens as `n` grows with fixed `m`: the filter
**saturates**. At some point almost every bit is 1, and then
every query returns "maybe present," which is useless. The
`test_saturation` demo in the code shows this effect.

**Rule of thumb:** budget about **10 bits per item** for a
false-positive rate near 1%.

## Bloom Filter vs Hash Set

| | Bloom filter | Hash set |
|---|---|---|
| Space per item | ~10 bits | ~64+ bytes |
| False positives | possible | none |
| False negatives | impossible | none |
| Remove an item | not easily | yes |
| Iterate contents | impossible | yes |

For a billion URLs, a Bloom filter costs ~1.25 GB; a hash set
costs ~64 GB. That is why databases use Bloom filters to
avoid disk lookups for keys that are definitely absent.

## Where Bloom Filters Are Used

- **Databases** (LevelDB, RocksDB, Cassandra): skip disk reads
  for keys that don't exist.
- **Content delivery**: check whether a URL is in the cache
  before hitting the origin server.
- **Malware scanners**: quickly check whether a file hash is
  known-bad, without storing the full list.
- **Browsers**: Chrome uses a Bloom filter to check for
  malicious URLs before querying the safe-browsing service.
- **Bitcoin SPV clients**: filter for transactions relevant to
  the wallet, at the cost of some privacy.

## Notes

- **Bit array size is fixed** at compile time (`BITS = 4096`).
  A production filter would size this at runtime based on the
  expected `n` and target FP rate.
- **`HASHES = 4`** is the sweet spot for these parameters.
  Too few hashes means clustering; too many means rapid
  saturation.
- **No deletion.** Clearing a bit would break other items that
  share it. A **counting Bloom filter** (replacing bits with
  small counters) supports deletion at the cost of space.
- **No iteration.** You cannot enumerate what is in the set
  because the set doesn't store the items.
- **`bf->inserted`** is bookkeeping only — it is not part of
  the Bloom filter, just a convenience for the demo.

## What You Can Extend

- **Runtime-configurable parameters** — take `m` and `k` as
  command-line arguments
- **A counting Bloom filter** — use `uint8_t` counters per slot
  and support removal
- **A scalable Bloom filter** — chain filters together with
  increasing size so you can keep inserting without saturating
- **Cuckoo filter** — a related data structure that supports
  deletion and has better space efficiency
- **Serialize to disk** — save `bits` to a file and reload
- **Compare with `std::set<std::string>`** — count how many
  disk I/Os the Bloom filter saves

## Why This Matters for Systems Programming

- **Probabilistic data structures** are how systems scale. Bloom
  filters, cuckoo filters, HyperLogLog, count-min sketch, and
  t-digest all trade a small error for enormous space savings.
- **Bit manipulation** is unavoidable in systems code — flag
  registers, inode bits, page-table entries, network headers
  all pack information into words.
- **Explicit false-positive reasoning** is the same skill used
  in designing caches, rate limiters, and eventually-consistent
  systems. "What kind of error can I tolerate, and how much?"
  is the central question.
- **Hash function choice** matters everywhere. Every Bloom
  filter, hash table, and checksum you will ever write depends
  on getting this right.

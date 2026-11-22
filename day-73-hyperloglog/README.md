# Day 73 — hyperloglog

**HyperLogLog** (HLL): estimate the number of distinct elements
in a stream using a fixed amount of memory, independent of how
many elements pass through.

## What HLL Does

You have a stream of items — clicks, IP addresses, user IDs,
URLs, whatever. You want to know: **how many distinct items
have I seen?**

The obvious solution is a set. Add each item to a hash set;
at the end, return the set's size. That's exact. But it uses
memory proportional to the number of distinct items — for a
million users, a few megabytes; for a billion, gigabytes.

HLL trades exactness for memory. With **16 KB**, it estimates
the cardinality of **any-sized stream** with about **0.81%
error**. That's more than enough for dashboards, capacity
planning, and analytics.

| Method | Memory | Error | Speed |
|--------|--------|-------|-------|
| Exact hash set | O(n) | 0 | Fast |
| HyperLogLog (m=16384) | 16 KB | ~0.81% | Fast |
| HyperLogLog (m=65536) | 64 KB | ~0.41% | Fast |
| Linear counting | ~O(n) | 0 | Slow |

Redis's `PFADD`/`PFCOUNT`, Google BigQuery, Presto,
Cloudflare analytics, and New Relic all use HLL (or a
close variant) for distinct-count queries.

## The Core Idea

Hash each element to 64 random bits. Two observations:

1. **The hash space is huge.** 64 bits means 2^64 possible
   values. For a set with few elements, collisions are
   astronomically rare.
2. **The leading-zero count follows a geometric
   distribution.** P(a random hash starts with k zeros) =
   2^(-k). So if you see a hash with 10 leading zeros, you
   expect to have seen around 2^10 = 1024 distinct hashes.

By tracking the **maximum leading-zero count** across all
hashes, you get a rough cardinality estimate. The problem:
this estimate has enormous variance. One unlucky high-rank
hash skews everything.

HLL's fix: use **m registers** instead of one. Each element
goes to one register based on part of its hash. Each register
tracks the max rank it has seen. At the end, combine the
registers with the **harmonic mean** (not arithmetic), which
is robust to outliers.

## The Algorithm, Step by Step

For `m = 2^p` registers:

**Add an element x:**

1. Compute `hash = H(x)` (64 bits).
2. Use the **top p bits** of `hash` as the register index
   (0..m-1).
3. Use the **remaining 64-p bits** to compute a "rank":
   count leading zeros, add 1.
4. Update `registers[idx] = max(registers[idx], rank)`.

**Estimate:**

1. Compute `Z = sum(2^(-registers[i]))` for all i.
2. `E = alpha_m * m^2 / Z`, where `alpha_m` is a
   precomputed bias constant (0.7213 / (1 + 1.079/m) for m
   above 128).
3. Apply small-range correction if `E` is small and many
   registers are zero.
4. Apply large-range correction if `E` is very large
   (approaching the hash space).

That's the whole algorithm. About 30 lines of core logic.

## Why Harmonic Mean

Suppose you have 16,384 registers, each with a value. You
want to combine them into one estimate.

**Arithmetic mean** is the wrong choice. If one register
has a very high value (an outlier), the arithmetic mean
gets pulled up. The estimate would over-shoot badly.

**Harmonic mean** is robust to outliers. It's dominated
by the **small** values. Since most registers reflect
"normal" hash behavior, and only a few reflect "unlucky"
behavior, the harmonic mean gives a stable estimate.

Specifically:

    Z = sum(2^(-register[i]))

This is the reciprocal of the harmonic mean of `2^register`.
If a register has value `r`, `2^(-r)` is tiny for large r.
So a few high-rank registers contribute negligibly to Z,
which keeps the estimator stable.

The multiplier `alpha_m * m^2 / Z` comes from the
mathematical expectation: for a set of size `n`, the
harmonic mean of `2^rank` over `m` registers is proportional
to `n^2 / m`. Solving for `n` gives the estimator.

## The Bias Constant

`alpha_m` corrects for the fact that the harmonic mean
estimate has a small systematic bias. The exact formula
depends on m:

- For m ≥ 128: `alpha_m = 0.7213 / (1 + 1.079/m)`
- For m = 16, 32, 64: precomputed constants (0.673, 0.697, 0.709)

The formula is from Flajolet et al. 2007 (the original
HLL paper). Without it, the estimate is off by a few
percent.

## Small-Range Correction

For small cardinalities (less than ~2.5·m), the raw HLL
estimate is biased high. This is because **most registers
are still zero** — the algorithm can't distinguish "no
elements" from "one element that happened to hash to a
low rank."

The fix is **linear counting**: count the number of zero
registers, and estimate

    n_LC = m * ln(m / V)

where V is the number of zero registers. This is accurate
for small n, and the algorithm switches to it when
`E <= 2.5 * m`.

This is why HLL doesn't need special handling for "few
elements" — linear counting handles it.

## Large-Range Correction

For very large cardinalities (close to 2^32 for a 32-bit
hash, or 2^64 for 64-bit), the estimate becomes
biased low. Why: if you've seen almost as many distinct
hashes as there are possible hashes, you start hitting
collisions and the rank distribution flattens.

The fix:

    if E > 2^32 / 30:
        E = -2^32 * ln(1 - E / 2^32)

This corrects the tail behavior. For our 64-bit hashes,
the correction would need to use 2^64 instead, but
the demo only tests up to 1,000,000 so the large-range
correction isn't exercised.

## Usage

    ./hyperloglog              run self-tests
    ./hyperloglog 10000        demo: 10000 random ints, estimate distinct

## Example

    $ ./hyperloglog 10000
    Estimating distinct in 10000 random integers (0..999)
      Estimated distinct: 984
      Expected distinct: about 1000 (1000 * (1 - e^(-n/1000)))

    $ ./hyperloglog 100
    Estimating distinct in 100 random integers (0..999)
      Estimated distinct: 94
      Expected distinct: about 95 (1000 * (1 - e^(-n/1000)))

The estimator is within a few percent of the true expected
count.

## Build

    gcc -Wall -Wextra -std=c99 -o hyperloglog hyperloglog.c -lm

Note the `-lm` — the estimator uses `log` and `exp` from libm.

## The Test Suite

| Test | What it verifies |
|------|------------------|
| 1. Small | Linear counting kicks in for n <= 2.5m |
| 2. Repeats | Repeated elements count once |
| 3. Medium | Raw estimate works for large n |
| 4. Strings | Works on non-integer inputs |
| 5. Accuracy | 10 trials at N=100,000, error within expected |
| 6. Memory | 16 KB total, vs. O(n) for exact |

Test 5 is the interesting one: it runs 10 independent
estimates and checks that both the average error and the
worst-case error are within expectations (~0.81% for
m=16384). This is the empirical validation that the
algorithm works as advertised.

## Skills Practiced

- **Fixed-size register array as a sketch** — memory
  independent of stream size
- **Hash-to-register** mapping via bit manipulation
- **Leading-zero extraction** — a fast bit operation
  (builtin `clz`, or a linear fallback)
- **Harmonic mean** — the key mathematical trick that
  reduces variance
- **Linear counting** — the small-range correction
- **Bias constant** — from the Flajolet et al. paper
- **Empirical accuracy testing** — comparing estimate to
  truth over many trials
- **Probabilistic reasoning** — "how confident are we?"

## Why HLL Instead of a Hash Set

For 1,000 distinct items, a hash set uses ~64 KB. HLL uses
16 KB and gives a 0.81%-accurate estimate. For 1M items,
hash set uses ~64 MB; HLL still 16 KB. For 100M items,
hash set uses ~6.4 GB (might not fit in RAM); HLL still 16 KB.

The tradeoff: exact set gives exact answer and supports
"is X in the set" queries. HLL gives approximate answer and
doesn't support membership. For counting-only use cases,
HLL wins on memory by orders of magnitude.

| Query | Exact set | HLL |
|-------|-----------|-----|
| Count | Exact | ~0.81% error |
| Is X present? | Yes | No (can't tell) |
| Union | Merge sets | Merge registers (max) |
| Memory at 1M | ~64 MB | 16 KB |

If you need membership testing, use a Bloom filter (Day 42).
If you need cardinality, use HLL. If you need both, use
both.

## Where HLL Is Used

- **Redis** — `PFADD`, `PFCOUNT`, `PFMERGE` commands
  (named after Philippe Flajolet, the algorithm's inventor)
- **Google BigQuery** — `APPROX_COUNT_DISTINCT`
- **Presto/Trino** — same function
- **Cloudflare Analytics** — unique visitors per day
- **New Relic, Datadog** — unique user counts
- **ClickHouse** — `uniq()` aggregate
- **Every A/B testing platform** — "how many unique users
  saw variant B?"

The "count distinct users" query at scale is HLL's
killer app. It's what makes "unique visitors" metrics
feasible for websites with billions of page views.

## Notes

- **m = 16384 is the standard choice** (Redis uses
  m=16384 for 12 KB with ~0.81% error; some systems
  use 2^14). Bigger m is more accurate but uses more
  memory. Doubling m halves the standard error.
- **HLL's error is probabilistic, not worst-case.** With
  p=14, the standard error is 1.04/sqrt(16384) ≈ 0.81%.
  About 68% of estimates are within 1 standard error,
  ~95% within 2, ~99.7% within 3. So a single estimate
  could be off by 2-3%. The test runs 10 trials and
  checks the aggregate.
- **Hashing matters.** HLL assumes the hash outputs are
  uniform random. A weak hash (e.g., a naive sum of bytes)
  would produce correlated hashes and the estimate would
  be biased. FNV-1a is adequate; SHA-256 is overkill but
  also works. Cryptographic hashes are sometimes used
  for adversarial robustness (an attacker can't craft
  inputs to skew the count).
- **Adversarial HLL is possible.** If an attacker knows
  the hash function, they can craft inputs that all hash
  to the same register, making the count appear 1 instead
  of a million. HLL++ and other variants use SipHash or
  similar keyed hashes to prevent this.
- **Registers are bytes.** Each register holds 0..64.
  `uint8_t` is sufficient. Total memory is m bytes.
  Sparse representations (HLL++) can be smaller still
  for small n, at the cost of more complex update logic.
- **The linear counting correction matters for correctness.**
  Without it, small cardinalities are systematically
  over-estimated. With it, they're accurate.
- **HLL doesn't support deletion.** You can't remove an
  element from the sketch (you don't know which register
  to decrement, and register values are max-based). To
  support deletion, you'd need a different structure
  (like a counting HLL, which trades memory for
  mutability).
- **Union is easy.** To combine two HLLs (estimate
  the distinct count of `A ∪ B`), take the max of
  corresponding registers. This is why HLL is used
  for distributed cardinality: each shard computes
  an HLL, then you merge.

## What You Can Extend

- **HLL++** — the Google variant with better bias
  correction and sparse representation
- **Sparse mode** — small-n representation using a map
  instead of full register array
- **Serialization** — save the registers to bytes and
  read back; compatible across machines
- **Union** — `hll_merge(a, b)` taking max of registers;
  use for distributed counting
- **Cardinality-at-time** — sliding window HLLs for
  "distinct in the last hour"
- **Adaptive p** — start small, grow m as needed
- **Compare with exact set** — benchmark memory and
  accuracy on a real stream
- **Test on real data** — count distinct IPs from a
  web log, or distinct user IDs from a session log

## Why This Matters for Systems Programming

- **Distinct counting is a universal metric.** Every
  analytics system, every capacity planner, every
  anomaly detector needs it. HLL makes it scale.
- **Probabilistic data structures are the theme.**
  HLL for cardinality, Bloom filter for membership,
  count-min sketch for frequency, t-digest for quantiles.
  These are the four classic sketches, and understanding
  one makes the others easy.
- **Fixed-size data structures enable predictable
  resource use.** A per-tenant HLL costs 16 KB, always.
  A per-tenant exact set could grow to gigabytes. In
  multi-tenant systems, predictable memory is critical.
- **The bit-manipulation approach generalizes.** Leading-
  zero counting (used here) is also the core of:
  hardware CLZ instructions, floating-point format
  parsing, and even the design of some CPU schedulers.
- **The bias-correction detail matters.** Naive HLL
  over-estimates small sets. Learning to check for
  small-range bias (and correct it) is a general
  lesson in statistical algorithms.

## Why This Matters for Cybersecurity

- **Distinct IPs / sessions / user agents** are standard
  security telemetry. HLL counts them without storing
  the raw identifiers, which matters for privacy and
  scale.
- **Adversarial HLL** (mentioned above) is a real
  concern for security telemetry. A determined attacker
  who knows your hash can evade HLL-based rate limiting
  or anomaly detection. Keyed hashes fix this.
- **Stream processing for detection** uses sketches
  heavily. Network intrusion detection systems (Zeek,
  Suricata) use HLL-style counting for "how many
  unique destinations from this source?"
- **The memory budget matters for edge devices.** A
  router or firewall can't store a set of billions of
  IPs. HLL's fixed 16 KB per counter is deployable.
- **HLL doesn't leak the input elements.** Only the
  aggregate count is recoverable from the sketch. This
  is a privacy feature — good for GDPR-compliant
  analytics.

## Why This Matters as a Portfolio Piece

- **"I implemented HyperLogLog"** is a specific
  achievement. It's a non-trivial algorithm from the
  research literature, and anyone in data engineering
  or distributed systems respects it.
- **The bit-level implementation** (register indexing,
  leading-zero counting) shows you can work at the
  primitive layer.
- **The bias-correction detail** shows you understand
  the algorithm well enough to know it doesn't work
  naively for small inputs.
- **The accuracy tests** are the empirical validation.
  Anyone can write code; testing that it works to the
  advertised accuracy is what makes the code trustworthy.
- **It pairs with Day 42 (`bloom_filter`).** Together
  they're the two most important probabilistic data
  structures. Having both shows depth in the
  probabilistic-algorithms space.

## The Probabilistic Data Structures Arc

| Day | Project | Problem | Memory |
|-----|---------|---------|--------|
| 42 | `bloom_filter` | Membership | 10 bits/item |
| **73** | **`hyperloglog`** | **Cardinality** | **16 KB total** |
| — | (future) count-min sketch | Frequency | O(width) |
| — | (future) t-digest | Quantiles | O(compression) |

These four sketches (Bloom, HLL, count-min, t-digest) cover
the four most common "aggregate queries" in streaming:
"is X present?", "how many distinct?", "how frequent is X?",
"what's the 99th percentile?" Learning one makes the
others approachable — they all use hashing, fixed memory,
and probabilistic reasoning.

## The Cloud & Networking Arc, Complete

Day 72 was the finale of the Cloud & Networking arc. Day 73
is the first day of the regular rotation after.

The arc's five entries (64, 66, 68, 70, 72) demonstrated
the systems-engineering stack from TCP up to containers.
Day 73 doesn't fit the arc's theme, but it fits the
portfolio: cardinality estimation is a fundamental
technique in the cloud (every analytics service uses it),
just as sockets and containers are.

The full arc + postlude:

| Day | Project | Theme |
|-----|---------|-------|
| 64 | `tcp_echo` | 🐧 Kernel |
| 65 | `simple_regex` | ⚙️ Pure C |
| 66 | `tcp_concurrent` | 🐧 Kernel |
| 67 | `minesweeper_solver` | 🎮 Game |
| 68 | `http_server` | ⚙️ Pure C |
| 69 | `padding_oracle_demo` | 🔐 Cyber |
| 70 | `round_robin_lb` | 🐧 Kernel |
| 71 | `bit_trie` | ⚙️ Pure C |
| 72 | `mini_container` | 🐧 Kernel |
| **73** | **`hyperloglog`** | **📐 Math** |

That's a 10-day burst that touched every theme, told a
coherent cloud story, and ended with a math-flavored
probabilistic structure. Good closure.

# Day 71 — bit_trie

A **bit trie** (also called a **binary trie**): a tree where
each level branches on one bit of the key. Used for exact
lookup, and — more importantly — for **longest-prefix matching**,
the operation behind IP routing.

## What a Trie Is

A trie stores keys as paths through a tree. Each node branches
on one unit of the key:

- A **character trie** branches on bytes: `a` → node, `b` → node, ...
- A **bit trie** branches on individual bits: `0` → left, `1` → right

For a 32-bit key, the path from root to leaf is 32 bits long.
For a 128-bit IPv6 key, 128 levels.

    root
    ├── 0
    │   ├── 0
    │   │   └── 1
    │   │       └── [value]
    │   └── 1
    │       └── 0
    │           └── [value]
    └── 1
        └── ...

Each `[value]` is a leaf storing a value for that exact key.
Nodes with no value are "internal" — they're only useful for
distinguishing keys that share a prefix.

The depth is `O(key_bits)` — 32 for IPv4, 128 for IPv6. Lookups
visit at most that many nodes, regardless of how many keys
are stored. So lookup is **O(key_bits)**, independent of `n`.

## Bit Extraction

Walking a key bit-by-bit is trivial with shifts and masks:

    for (int i = bits - 1; i >= 0; i--) {
        int b = (key >> i) & 1;
        /* descend into cur->child[b] */
    }

That iterates from the **most significant bit** (i = bits-1)
to the least. The convention matters: if you ever insert and
look up using the same bit order, any order works. But if you
mix orders (or compare against a system that uses MSB-first),
you'll get wrong results.

MSB-first is the convention because it matches **prefix**
semantics. Storing `/8` means "the first 8 bits are fixed."
A prefix of `10.0.0.0/8` shares its first 8 bits with every
`10.x.x.x` address. That's why routers use prefix length:
"how many leading bits are part of this route?"

MSB-first means all routes that share a prefix are adjacent
in the trie — they form a subtree. That's the whole point.

## Operations

### Insert

    walk the key's bits from MSB to LSB
    create child nodes as needed
    at the leaf, set has_value = 1 and value = v

### Lookup

    walk the key's bits from MSB to LSB
    if any child is missing, return "not found"
    at the leaf, return has_value ? value : "not found"

### Longest prefix match

This is the operation that makes bit tries special.

    walk the key's bits from MSB to LSB
    track the deepest node we've seen that has has_value
    return that node's value

At each node with `has_value`, the path from root to that node
is a **prefix** of the key that has a stored value. We keep
walking because a longer prefix (deeper node) that also has
a value is a more specific match. When we can't walk further,
the last value we saw is the longest prefix.

For routes:

    trie_insert(root, 10.0.0.0/8,  value=10)
    trie_insert(root, 10.1.0.0/16, value=101)
    trie_insert(root, 10.1.2.0/24, value=1012)

    trie_longest_prefix(root, 10.1.2.5)
      walks: 10.0.0.0/8   (value=10)    -> best so far
             10.1.0.0/16  (value=101)   -> best so far
             10.1.2.0/24  (value=1012)  -> best so far
             10.1.2.5/32  (no value)    -> stop
      returns: 1012 (the /24 route)

That's exactly what a router does: given a destination IP,
find the most specific matching route. The `/0` default
route matches everything, the `/32` host route is exact.

### Delete

Walk to the leaf, clear `has_value`, then walk back up
pruning any node that has no value and no children. This is
how a trie stays compact after deletions.

    trie_delete(root, 10.1.2.0/24):
        walk down to /24 node
        clear has_value
        walk up: /24 node has no children, no value -> free it
        walk up: /16 node has other children, keep it
        ...

The pruning loop walks the path in reverse and stops as
soon as it finds a node that's still needed.

## Where This Structure Is Used

**IP routing.** The Linux routing table is a bit trie (the
"LC-trie," a compressed variant). BGP route lookups are
longest-prefix matches. Every packet that crosses the
internet does one.

**Firewall rule matching.** A firewall stores rules keyed
on source/dest IPs, ports, and protocols. Matching a packet
against rules with wildcards is a longest-prefix match
problem. Open vSwitch, nftables, and P4 all use tries.

**Intrusion detection.** Snort and Suricata match packets
against signature patterns, which are stored in tries and
prefix trees.

**Packet classification.** Given a 5-tuple (src IP, dst IP,
src port, dst port, protocol), find the rule that applies.
This is "multi-dimensional longest prefix match" — usually
implemented as nested tries or specialized hardware.

**Memory allocators.** The Linux `slab` allocator uses a
trie-like structure to find free memory blocks.

**Autocomplete.** Not bit tries, but character tries.
Type "hel" and get "hello", "help", "helper". Same
structure, same walk, different "alphabet."

## Usage

    ./bit_trie

Runs six self-tests and prints `ok` or `FAIL`.

## Build

    gcc -Wall -Wextra -std=c99 -o bit_trie bit_trie.c

## The Test Suite

| Test | What it verifies |
|------|------------------|
| 1. Basic | Insert + lookup of exact 32-bit keys |
| 2. LPM | IPv4 routes at /0, /8, /16, /24, /32 |
| 3. Delete | Removing keys and pruning empty branches |
| 4. Overwrite | Inserting the same key twice updates the value |
| 5. Empty | Operations on an empty trie behave correctly |
| 6. Short keys | Mixed-length keys in the same trie |

Test 2 is the most interesting: it builds a mini routing
table and verifies that `longest_prefix` returns the correct
route for six different IPs.

## Test 2 in Detail

The trie is populated with:

| Prefix | Value |
|--------|-------|
| `0.0.0.0/0` | 1 |
| `10.0.0.0/8` | 10 |
| `10.1.0.0/16` | 101 |
| `10.1.2.0/24` | 1012 |
| `10.1.2.3/32` | 10123 |

Then five lookups are tested:

| Destination | Expected prefix | Expected value |
|-------------|-----------------|----------------|
| `0.0.0.0` | `/0` (0 bits) | 1 |
| `11.0.0.1` | `/0` | 1 |
| `10.255.255.255` | `/8` (8 bits) | 10 |
| `10.1.255.255` | `/16` (16 bits) | 101 |
| `10.1.2.255` | `/24` (24 bits) | 1012 |
| `10.1.2.3` | `/32` (32 bits) | 10123 |

The interesting cases are `10.255.255.255` (matches `/8`
because 10.0.0.0/8 is the most specific route that includes
it) and `10.1.255.255` (matches `/16` for the same reason).
`11.0.0.1` doesn't match any specific route, so it falls
through to `/0`, the default.

That's exactly what a real router does. The default route
at /0 catches everything; more specific routes override it.

## Notes

- **No compression.** A plain bit trie has one node per bit
  of every stored key. For a sparse set of keys, that's a lot
  of empty nodes. Real implementations use a **Patricia trie**
  or **radix tree** — they compress single-child chains into
  one node labeled with the whole prefix. Linux's LC-trie is
  a compressed variant of this.
- **No path compression by default.** Inserting `10.0.0.0/8`
  creates 8 nodes (one per bit of 10). Inserting
  `10.0.0.0/8` and `10.0.0.0/9` adds one more node. The
  memory cost is `O(keys × key_bits)` worst case, which is
  33 nodes per key for 32-bit keys.
- **Fixed key length.** The `bits` argument to each function
  tells the trie how many bits to use. All keys in one trie
  should have the same bit length. The test for short keys
  cheats — it mixes 8-bit and 16-bit keys in one trie — but
  in practice you pick one and stick with it.
- **`path` array in `trie_delete` is limited to 64 bits.**
  For IPv6 (128-bit keys), you'd need a 128-entry array, or
  use recursion with an explicit stack. This file targets
  IPv4-sized keys.
- **No value type flexibility.** Values are `int`. A general
  trie would use `void *` or `uint64_t`. The change is
  mechanical — replace `int value` with `void *value` and
  adjust the accessor.
- **Lookup doesn't return "how deep" the match was.** For
  exact lookup, it returns just the value. For LPM, it
  returns both the value and the prefix length. That
  asymmetry is deliberate — a router needs to know which
  route matched (for logging, counters, and so on), not just
  what value it maps to.
- **No concurrency.** This is single-threaded. A concurrent
  version would need read-write locks or RCU (read-copy-update),
  which is what Linux's actual routing trie uses.

## What You Can Extend

- **Patricia trie / radix tree** — compress single-child chains
- **128-bit support** — for IPv6, with `__int128` or two `uint64_t`s
- **Multi-dimensional LPM** — match on (src IP, dst IP, port)
  simultaneously; packet classification
- **Route aggregation** — given a list of routes, produce a
  minimal trie (the "prefix compression" problem)
- **`void *` values** — store pointers instead of ints
- **Iteration** — walk the whole trie in key order
- **Prefix counting** — how many routes share a given prefix?
- **Compare with `ip route`** — load the output of
  `ip route show` into this trie and look up destinations
- **Test against real BGP data** — the RouteViews project
  publishes BGP table dumps; parse one and see how many
  prefixes are in a live routing table

## Why This Matters for Systems Programming

- **IP routing** is the canonical use of longest-prefix
  matching. Every packet that crosses the internet does
  an LPM lookup at every hop.
- **Every firewall** that handles packet filtering is doing
  some form of prefix matching. Understanding the trie
  underneath makes firewall rules comprehensible.
- **Tries generalize to many "walk" algorithms.** A trie is
  to a set of keys what a tree is to a list — it organizes
  for lookup, at the cost of memory.
- **Compression variants are essential in production.**
  Plain bit tries waste memory; compressed tries (Patricia,
  LC-trie) are what real systems use. This file teaches
  the concept; the compression is a mechanical extension.
- **The step-order comment** at the top of the file makes
  the algorithm readable before the code. That's how the
  file is structured: plan first, code second.

## Why This Matters for Cybersecurity

- **Packet classification** for firewalls, IPS, and DPI
  uses multi-dimensional tries. Understanding single-dimension
  LPM is the first step.
- **Network attacks** often exploit routing bugs. A
  route-hijack attack is a BGP prefix announcement that
  redirects traffic — understanding prefix matching is
  prerequisite to understanding the attack.
- **Fuzzer input matching** — modern fuzzers (AFL++, libFuzzer)
  use coverage maps indexed by tries to guide exploration.
  The same data structure, different application.
- **Malware C2 matching** — IDS signature lists are tries.
  A URL like `malware.com/c2` matches as a prefix, catching
  all paths under it.
- **No value at internal nodes.** This file lets any node
  have a value, which is how routes at /8, /16, /24 coexist.
  In a security context, "does the firewall have a rule at
  /16 that matches this IP" is the question LPM answers.

## Why This Matters as a Portfolio Piece

- **"I wrote a bit trie with longest-prefix match"** is a
  specific, verifiable achievement. Anyone in networking or
  systems immediately understands what it means.
- **The IP-routing test cases** are the kind of domain
  knowledge that distinguishes "I implemented a data
  structure" from "I implemented a data structure *for
  this problem*." The tests show the trie in its natural
  habitat.
- **The code is small** — ~250 lines — but the concept
  (a data structure used by every router on the internet)
  is huge. That's a good portfolio ratio.
- **It connects to the networking arc.** Days 64-70 were
  TCP and HTTP; Day 71 is the data structure under IP
  routing. A reviewer sees the arc continuing: sockets,
  concurrency, protocol, load balancing, and now the data
  structure behind the routing table.
- **The README's "What You Can Extend"** shows the path to
  production (Patricia trie, IPv6, multi-dimensional
  matching) without pretending this file already has it.

## The Data Structures Arc

| Day | Project | Lookup | Extra |
|-----|---------|--------|-------|
| 09 | `linked_list` | O(n) | Insertion |
| 13 | `binary_search` | O(log n) | Sorted array |
| 21 | `binary_tree` | O(log n) avg | BST |
| 28 | `hash_table` | O(1) avg | Chaining |
| 37 | `ring_buffer` | O(1) front/back | Fixed capacity |
| 42 | `bloom_filter` | O(k) prob | No false negatives |
| 62 | `lru_cache` | O(1) | Eviction |
| **71** | **`bit_trie`** | **O(bits)** | **Longest prefix** |

That's a complete tour of the classic data structures:
arrays, linked lists, trees, hashes, tries, and their
probabilistic relatives. Each with a specific lookup
complexity and a specific kind of extra operation.

## The Cloud & Networking Arc

| Day | Project | Focus |
|-----|---------|-------|
| 64 | `tcp_echo` | sockets, ports, client-server |
| 66 | `tcp_concurrent` | fork + poll, scalable servers |
| 68 | `http_server` | HTTP protocol, static file serving |
| 70 | `round_robin_lb` | load balancing, health checks |
| **72** | **`mini_container`** | **chroot, unshare, containers** |

Day 71 is the "interleaved" day between the LB and the
container — a chance to breathe and look at a data structure
that ties to the networking theme without being another
server. Day 72 closes the arc with the biggest entry:
process isolation via `chroot` + `unshare` + `clone`.

After Day 72, the arc is complete and the regular rotation
resumes. The repo will have a coherent 9-day story: TCP →
concurrency → HTTP → LB → routing data structure → container.
That's a real systems-engineering narrative.

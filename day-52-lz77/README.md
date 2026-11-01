# Day 52 — lz77

**LZ77**: the sliding-window compression algorithm behind DEFLATE,
gzip, PNG, ZIP, LZMA, LZ4, and every modern general-purpose
compressor.

## What Is LZ77?

The core observation is simple:

> When you see a repeated substring, replace it with a pointer
> to where it appeared before.

The classic example: `abcabcabcabc` (12 bytes) becomes
`abc` followed by `(copy 3 bytes from 3 back, copy 3 from 3 back,
copy 3 from 3 back)`. The 12 bytes compress to about 6.

LZ77 (Lempel-Ziv, 1977) formalized this as a **sliding window**
algorithm:

1. Keep a window of the last N bytes (the "history").
2. At each position, find the longest match of upcoming data
   within the window.
3. Emit a **backreference** — (offset, length) — plus the
   **literal** byte that follows the match.
4. Advance past the match and the literal.

The output is a sequence of `(offset, length, literal)` triples
that decode back to the original input exactly.

## Why This Is Different from Huffman

**Huffman coding (Day 47)** compresses by encoding symbols
shorter or longer based on frequency. It exploits **symbol
skew**.

**LZ77** compresses by finding repeated substrings and
replacing them with backreferences. It exploits **repetition
in sequence**.

The two are complementary:

- A text file has both skewed frequencies and repeated words —
  both algorithms help.
- A binary file might have uniform frequencies but structural
  repetition — LZ77 helps, Huffman doesn't.
- A random file has neither — both produce overhead.

**DEFLATE** = LZ77 followed by Huffman. That's the format
inside gzip, PNG, ZIP, and most HTTP compression. The two
algorithms in this repo (Day 47 and Day 52) are literally
the two halves of DEFLATE.

## How This Implementation Works

**Encoding** produces a header (the original size) followed by
triples:

    Byte 0: offset   (0 = literal, 1..255 = backreference)
    Byte 1: length   (0 = literal, 1..255 = match length)
    Byte 2: literal  (the byte that follows the match)

A **literal** triple is `(0, 0, X)`, meaning "emit byte X."

A **match** triple is `(offset, length, X)`, meaning "copy
`length` bytes from `offset` back, then emit X."

The header ensures the decoder knows when to stop (otherwise
trailing zero-length padding can't be distinguished from data).

**Decoding** is trivial:

    For each triple:
        Copy `length` bytes from `current - offset` forward.
        Append `literal`.

No lookup, no tree, no hashing. LZ77 decoding is faster than
encoding, which is why it's used for read-heavy workloads
(bootloaders, image display).

## The Match-Finding Loop

For each position `pos` in the input:

1. Look back up to `WINDOW_SIZE` bytes.
2. For each start position, count how many bytes match
   forward from `pos`.
3. Keep the longest match.

    for (start = window_start; start < pos; start++):
        len = 0
        while (data[start + len] == data[pos + len] and len < MAX_MATCH):
            len++
        if len > best_length:
            best_length = len
            best_offset = pos - start

This is **O(window_size × max_match)** per position, which is
fine for a demo but quadratic in the worst case. Real LZ77
implementations use **hash tables** or **suffix trees** to find
matches in O(1) per position:

- **DEFLATE** (gzip): hash chains — hash the next 3 bytes,
  chain through matching positions
- **LZMA** (xz, 7z): binary trees on match length
- **LZ4** (real-time): a single hash table with direct entry
- **Zstandard**: multiple hash tables + FSE entropy coding

The algorithm in this file is the **conceptually correct** version;
real implementations replace the inner loop with something
smarter.

## Usage

    ./lz77 encode < input.txt > output.lz77
    ./lz77 decode < output.lz77 > restored.txt
    diff input.txt restored.txt      # must be identical

    ./lz77 test                      # run self-tests

## Build

    gcc -Wall -Wextra -std=c99 -o lz77 lz77.c

## Example

    $ echo -n "the quick brown fox jumps over the lazy dog" > in.txt
    $ ./lz77 encode < in.txt > in.lz77
    $ ./lz77 decode < in.lz77 > out.txt
    $ diff in.txt out.txt && echo "match"
    match
    $ ls -l in.txt in.lz77
    -rw-r--r-- 43 in.txt
    -rw-r--r-- 132 in.lz77

The encoded file is **larger** than the input — because this
string has no long repeats and our triples cost 3 bytes each.
That's expected. LZ77 only wins on repetitive data.

Try it on a more realistic input:

    $ seq 1 1000 | ./lz77 encode > seq.lz77
    $ ls -l seq.lz77
    # roughly half the original size

The more repetition, the better LZ77 does.

## Skills Practiced

- **Sliding window** — the fundamental LZ77 data structure
- **Backreference encoding** — (offset, length) pointers
- **Triple-based file format** — header + triples
- **Naive O(n×window) match finding** — correct but slow
- **Decode-side triviality** — why compression formats are
  asymmetric
- **Realistic size analysis** — knowing when compression helps
  and when it doesn't

## Why LZ77 Alone Isn't Enough

LZ77 produces **triples**, and each triple costs 3 bytes. That's
very inefficient — real LZ77 uses **bit-packing**:

| Approach | Per triple | Compression |
|----------|-----------|-------------|
| Our version | 3 bytes | Poor |
| Bit-packed | ~2–3 bytes | Better |
| Bit-packed + Huffman | ~1–2 bytes | DEFLATE |

DEFLATE takes the (offset, length) pairs and Huffman-codes them.
The length is coded in a small alphabet (29 possible values),
the offset in a larger one (30 values with bit-packed tails),
and the literals in a third alphabet. Combined with a small
Huffman table per block, this gets to ~30–40% of the original
size on typical text.

**Our version** gets to about 50–60% on repetitive data, because
the triples are uncompressed. That's the whole gap between
"toy LZ77" and "real compression."

## LZ77 + Huffman = DEFLATE

This is the bridge back to Day 47. Take the (offset, length,
literal) triples this program produces, then:

1. Group all offsets into one stream.
2. Group all lengths into another.
3. Group all literals into a third.
4. Huffman-code each stream separately.

That's DEFLATE. That's gzip. That's the compression inside PNG,
ZIP, HTTP `Content-Encoding: gzip`, and everything else.

If you want to try it yourself: modify this program to emit
three separate arrays (offsets, lengths, literals) and then
run Day 47's Huffman code on each. The result should be within
a few percent of real gzip.

## Notes

- **The window is 255 bytes.** Real LZ77 uses 32 KB windows
  (DEFLATE) or larger. A larger window catches more repeats;
  a smaller one is faster to search. 255 lets us fit the
  offset in a byte.
- **The maximum match is 255 bytes.** Same tradeoff.
- **Minimum match is 3 bytes.** A 2-byte match costs 3 bytes
  in a triple and saves 2 — no win. So matches shorter than 3
  are always emitted as literals.
- **The naive matcher is slow** — O(n × window × match). For
  a 1 MB file, that's about 65 billion comparisons. Real
  implementations use hash chains to reduce this by ~100×.
- **Edge case at end of input**: the last match might extend
  past the end of the file. The literal after the match would
  be the next byte, but there isn't one. The encoder uses `0`
  and the decoder stops at `expected_size` bytes, so the extra
  `0` is discarded. This is the standard trick.
- **No error detection.** A corrupted `.lz77` file produces
  garbage, not an error message. Real formats add CRC32.

## What You Can Extend

- **Faster match finding** with a hash table of 3-byte prefixes
- **Combine with Day 47's Huffman** to produce a real DEFLATE
- **Bigger window** (32 KB, matching DEFLATE's default)
- **`run-length encoding` preprocessor** — replace runs of the
  same byte before LZ77 (helps very repetitive data)
- **Compare with gzip** on real files
- **A streaming decoder** that doesn't need the whole file in
  memory
- **CRC32 checksum** in the header
- **Integer overflow protection** — validate `out_len - offset >= 0`
  before reading; the current code would underflow on malformed input

## Why This Matters as a Portfolio Piece

- **LZ77 is one of the most important algorithms of the 20th
  century.** It's in literally every compressed file you've
  ever seen, and it's the foundation of compression as a
  field.
- **The pairing with Day 47 (`huffman_coding`) is a real
  achievement.** LZ77 + Huffman = DEFLATE. You now have both
  halves of the algorithm that runs the internet.
- **Understanding both compression paradigms** (frequency-based
  and repetition-based) makes you fluent in the tradeoffs that
  matter for real systems: speed vs ratio, memory vs
  correctness, streaming vs batch.
- **The gap between "works" and "fast" is visible.** The naive
  matcher in this file is correct but slow. A senior engineer
  would spot the O(n × window) immediately and reach for a hash
  table. That instinct is what distinguishes good systems
  engineers.

## Why This Matters for Systems Programming

- **Every compressed file** you handle uses some variant of
  LZ77. Understanding the format means understanding what
  your tools are doing.
- **The sliding-window pattern** appears everywhere: TCP flow
  control, signal processing, image convolution, and yes,
  compression.
- **Backreferences in data** are the same mechanism behind
  database foreign keys, symbolic links, and version control
  delta encoding (git's pack format uses a variant of LZ77).
- **Compression is the single biggest optimization available
  to a network system.** A 3× reduction in bytes on the wire
  is a 3× reduction in bandwidth, a 3× reduction in latency
  on slow links, and a 3× reduction in storage. Every web
  server has LZ77 somewhere in its stack.

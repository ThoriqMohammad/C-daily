# Day 47 — huffman_coding

**Huffman coding**: a variable-length prefix code that gives the
shortest bit-strings to the most frequent symbols. It is the
compression layer inside DEFLATE (gzip, zlib, PNG, ZIP).

## What Huffman Coding Does

Fixed-width encodings treat every symbol equally: ASCII uses 8
bits for `e` and 8 bits for `z`, even though `e` appears 100
times more often in English. Huffman coding assigns shorter
codes to frequent symbols:

    Letter   Frequency in English  Code (example)
    ------------------------------------------------
    e        12.7%                 10
    t         9.1%                 00
    a         8.2%                 110
    ...
    z         0.1%                 11110

The result: text typically compresses to ~55–65% of its size
with Huffman coding alone. Combined with LZ77 (which finds
repeated substrings), the result is DEFLATE — what gzip, PNG,
and ZIP use to compress data.

## How It Works

1. **Count** the frequency of each symbol in the input.
2. **Build a min-heap** of leaf nodes, one per symbol.
3. **Repeatedly merge**: pop the two smallest nodes, create a
   parent with their combined frequency, push it back.
4. **The last node** is the root of the tree.
5. **Assign codes**: left edge = 0, right edge = 1. The code
   for each leaf is the path from root to that leaf.

Because every leaf is at a different depth of the tree, **no
code is a prefix of another**. That's what makes the code
uniquely decodable without separators — the decoder reads bits
until it reaches a leaf, emits that symbol, and starts over.

## Example

Suppose the input is `AAAABBCC`. Frequencies:

    A: 4,  B: 2,  C: 2

Building the tree:

    Step 1: heap = [B:2, C:2, A:4]
    Step 2: pop B:2, C:2, push BC:4    -> heap = [A:4, BC:4]
    Step 3: pop A:4, BC:4, push ABC:8  -> heap = [ABC:8]

Tree:

         ABC:8
        /     \
     A:4      BC:4
             /    \
          B:2    C:2

Codes:

    A -> 0
    B -> 10
    C -> 11

Encoding `AAAABBCC`:

    A A A A B  C  B  C
    0 0 0 0 10 11 10 11

    10 bits total (vs 16 bits for 8-bit fixed encoding).
    Ratio: 10/16 = 62.5%

## Usage

The program is a working **compress/decompress** tool:

    ./huffman encode < input.txt > output.huf
    ./huffman decode < output.huf > restored.txt

    diff input.txt restored.txt   # should be identical

    ./huffman test                # run all self-tests

## Build

    gcc -Wall -Wextra -std=c99 -o huffman huffman.c

## File Format

The encoded output is:

| Bytes | Content |
|-------|---------|
| 4 | magic `HUF1` |
| 8 | original length (big-endian) |
| 1024 | 256 × 4-byte frequency table (big-endian) |
| ... | bit-packed Huffman stream (padded with zeros to byte boundary) |

The header is 1036 bytes, so **small files may compress *up***.
Huffman coding is a net win for files above a few KB; for
files smaller than the header, the output is bigger. This is
why real compression tools add other layers (LZ77, run-length,
or a smaller header format for small inputs).

## Skills Practiced

- **Min-heap** implemented as a sorted list (fast enough for 256 symbols)
- **Binary tree construction** from a greedy algorithm
- **Recursive tree traversal** to generate codes
- **Bit packing** — writing single bits into a byte stream
- **Bit reading** — reconstructing bits from a byte stream
- **Frequency analysis** — counting bytes in a stream
- **Variable-length encoding** — the foundation of compression
- **Binary file format design** — header + payload, endianness

## Why "Prefix-Free" Matters

If the code for `A` were `0` and the code for `B` were `01`,
a decoder reading `01` could not tell whether it meant `A`
followed by `1`, or `B`. Huffman coding guarantees this never
happens: **the code for any symbol is never a prefix of the
code for any other symbol**.

The `test_code_property` test verifies this explicitly by
checking every pair of codes for prefix overlap.

## Optimality

Huffman coding is **optimal among prefix codes**: no other
prefix code can achieve a shorter expected length for the same
input frequencies. This was proved by Huffman in 1952 and is
a classic result in information theory.

It is *not* optimal among all codes — arithmetic coding can do
better, because it encodes fractional bits per symbol. But
Huffman is faster and simpler, which is why DEFLATE uses it.

## Bit Packing

The code is streamed into bytes one bit at a time:

    bw_put_bit(&bw, 0 or 1):
        cur = (cur << 1) | bit
        bits++
        if bits == 8:
            write cur to file
            cur = 0
            bits = 0

    bw_flush(&bw):
        if bits > 0:
            cur <<= (8 - bits)   /* pad with zeros */
            write cur
            cur = 0
            bits = 0

The decoder mirrors this:

    br_get_bit(&br):
        if bits == 0:
            cur = read one byte
            bits = 8
        bit = (cur >> 7) & 1
        cur <<= 1
        bits--
        return bit

Both are tiny, and both are the standard way to do bit-level
I/O in C. Every compression format uses the same pattern.

## Notes

- **The tree is not stored in the file.** Instead, we store the
  frequency table and rebuild the tree on decode. This is
  simpler than serializing the tree and costs only 1024 bytes.
- **The bit stream is zero-padded** to a byte boundary at the
  end. The decoder stops after `orig_len` symbols, so trailing
  padding bits are never consumed.
- **Single-symbol files are special-cased.** A file of all `A`s
  would produce a tree with one node and no code; we give it
  an empty code and emit one 0 bit per symbol. The decoder
  knows to read one bit and emit the symbol each time.
- **Empty files** compress to just a header. The decoder sees
  `orig_len = 0` and produces no output.
- **The heap is a sorted list.** For 256 symbols, this is fine.
  A real implementation would use a binary heap for O(log n)
  operations instead of O(n).
- **No error detection.** A corrupted `.huf` file will produce
  garbage on decode. Real formats include checksums (CRC32,
  Adler32) — see DEFLATE.

## What You Can Extend

- **Canonical Huffman codes** — reorder the codes so only the
  code lengths need to be stored (this is what DEFLATE does)
- **LZ77 preprocessor** — find repeated substrings first, then
  Huffman-code the LZ77 output; this is DEFLATE
- **Arithmetic coding** — encode fractional bits per symbol,
  beating Huffman's 1-bit minimum
- **Adaptive Huffman** — rebuild the tree as data arrives,
  removing the two-pass requirement
- **CRC32 checksum** — detect corruption
- **A CLI compressor** with a real filename argument
- **Compare sizes** on real files: a text file, a binary,
  a JPEG — Huffman helps the first, does nothing for the last
  (JPEG is already compressed)

## Why This Matters for Pure C Programming

- **Bit manipulation** at the byte level is a recurring need:
  network protocols, file formats, compression, image encoding.
  Every one of these uses the bit writer / bit reader pair.
- **Tree construction from a priority queue** appears in
  scheduling, event simulation, and every greedy algorithm
  textbook.
- **The prefix-free property** is a beautiful example of an
  invariant enforced by construction — you cannot accidentally
  produce a bad Huffman code if you build it the right way.
- **Two-pass algorithms** (count, then encode) are a common
  pattern in compression, statistics, and machine learning.
  This implementation makes the two passes explicit.
- **The header + payload + checksum** layout is the shape of
  every real file format you'll ever implement.

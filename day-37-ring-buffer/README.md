# Day 37 — ring_buffer

A fixed-capacity **ring buffer** (circular queue) for integers,
with O(1) push and pop and no shifting of elements.

## What Is a Ring Buffer?

A ring buffer stores items in a fixed-size array and reuses the
space by wrapping around. Two indices mark the ends:

    head -> next item to read
    tail -> next slot to write

Both advance modulo the array length, so the structure never has
to move elements. When `head` and `tail` reach the end of the
array, they wrap back to index 0.

    cap = 5
    [ _ | _ | _ | _ | _ ]
      ^head
      ^tail           (empty)

    push 10, 20, 30
    [ 10 | 20 | 30 | _ | _ ]
      ^head       ^tail

    pop 10
    [ 10 | 20 | 30 | _ | _ ]
              ^head ^tail

## The Full-vs-Empty Problem

`head == tail` means **empty**. But if you fill the array
completely, `tail` also catches up to `head` — so the two
states look identical.

There are three common ways to fix this:

| Approach | Cost |
|----------|------|
| Keep a separate `count` field | extra bookkeeping |
| Track a `full` flag | easy to forget to clear |
| **Waste one slot** (used here) | one unusable slot |

This implementation uses the **wasted-slot** approach: the
allocated array is `capacity + 1` long, and the buffer is
declared full when `(tail + 1) % cap == head`. One slot is
always unused, but the code stays simple.

## Usage

    ./ring_buffer

Runs three built-in tests and prints `ok` or `FAIL` for each
assertion.

## Build

    gcc -Wall -Wextra -std=c99 -o ring_buffer ring_buffer.c

## Skills Practiced

- **Struct-based design** — one type, one set of operations
- **Modular arithmetic** for circular indexing
- **`malloc` / `free`** — allocating and releasing the array
- **Full vs empty** without ambiguity
- **Writing self-tests** in `main()` with a tiny `check()` helper

## API

| Function | Purpose |
|----------|---------|
| `rb_create(capacity)` | allocate a new buffer |
| `rb_destroy(rb)` | free the buffer |
| `rb_push(rb, value)` | add at the back; `-1` if full |
| `rb_pop(rb, &out)` | remove from the front; `-1` if empty |
| `rb_empty(rb)` / `rb_full(rb)` | state checks |
| `rb_size(rb)` | current item count |

Every operation returns `int` (`0` success / `-1` failure) so
the caller can never accidentally ignore an error.

## Why Wrap With `% cap`?

The index arithmetic is the whole trick:

    tail = (tail + 1) % cap;

Without the modulo, `tail` would run off the end of the array.
The modulo makes the index **wrap** back to 0 when it reaches
`cap`. This works because `cap` is a runtime value stored in
the struct — no branching needed.

An equivalent older trick is `if (++tail == cap) tail = 0;`.
Both are fine; the modulo is shorter and clearer.

## What You Can Extend

- Add `rb_peek()` to read the front without removing it
- Make it hold `void *` to store arbitrary data
- Add `rb_resize()` for growable ring buffers
- Use `memcpy` to batch push and pop multiple items
- Wrap a log file: write to disk and drop the oldest entry

## Why This Matters for Systems Programming

- Ring buffers are the shape of **I/O queues** everywhere:
  audio playback, keyboard input, network receive rings.
- Kernel trace systems (ftrace, perf) use per-CPU ring buffers.
- The full-vs-empty trick is a standard interview question and
  a standard source of off-by-one bugs in production.
- The pattern generalizes: any fixed-capacity FIFO with O(1)
  operations works this way.

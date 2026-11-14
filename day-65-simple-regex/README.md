# Day 65 — simple_regex

A minimal regular expression engine supporting `.`, `*`, `+`,
`?`, `^`, and `$`.

## What a Regex Engine Does

A regex engine answers one question: **does this pattern match
this string?**

The pattern is a small program describing a set of strings.
The engine is an interpreter for that program. Every `grep`,
`sed`, `awk`, `perl -e`, and JavaScript `.test()` call runs
this kind of interpreter under the hood.

The engine in this file is deliberately minimal:

| Syntax | Meaning |
|--------|---------|
| `a` | literal 'a' |
| `.` | any single character |
| `*` | zero or more of the preceding atom |
| `+` | one or more of the preceding atom |
| `?` | zero or one of the preceding atom |
| `^` | anchor at start of string |
| `$` | anchor at end of string |

No capture groups, no character classes (`[a-z]`), no
alternation (`a|b`), no repetition counts (`{3,5}`). That's
enough to be useful and small enough to read fully in one
sitting.

## The Backtracking Approach

The classic recursive approach (Russ Cox calls this
"the elegant one") directly translates the grammar of
patterns into C functions:

    match_here(pattern, string):
        if pattern is empty: return string is empty
        if pattern[1] == '*': try zero-or-more of pattern[0]
        if pattern[1] == '+': try one-or-more of pattern[0]
        if pattern[1] == '?': try zero-or-one of pattern[0]
        else: pattern[0] must match string[0], recurse on rest

The key insight: **backtracking**. When the pattern has a
`*` (zero or more), the engine tries the greedy option first
(match as much as possible), then on failure tries shorter
matches. For `a*b` against `aaab`:

- Try 3 a's then b: match. Return success.
- (If that failed, try 2 a's then b, etc.)

The recursion has this shape:

    a*b  against  aaab
    ├── try "aaab" then b:   aaa != b, fail
    ├── try "aa" then b:     aa + b matches, SUCCESS

For a simple pattern, backtracking is fast and correct. For
a pattern with multiple `*`s, it can explode exponentially.

## Catastrophic Backtracking

The classic "regex bomb" pattern:

    a?a?a?a?a?...a?b

against a long string of a's. Each `a?` can match or not
match — 2 choices. With N `a?` atoms, there are 2^N possible
paths through the recursion. If the string has no `b`, none
of them match, and all 2^N are explored.

For N = 30, that's **1 billion** recursive calls. For a 30-byte
input, on any machine. This is **ReDoS** (Regular Expression
Denial of Service), and it's a real vulnerability class that
has affected everything from nginx to Java's regex engine to
web frameworks.

The benchmark in this file uses `a?a?a?a?a?b` against `aaaaaaaaa`
(9 a's) — that's 512 paths. Small enough not to hang, big
enough to feel slow.

## Thompson's NFA: The Alternative

In 1968, Ken Thompson (yes, the Unix one) described a
different approach: **compile the pattern to an NFA** and
simulate it with all states active in parallel.

The NFA is a graph of states:

- One start state
- One accept state
- Edges labeled with characters or with `epsilon` (jump)

Matching an NFA means tracking the **set of states the NFA
could be in** after reading each character. At each step:

    next_states = union of all states reachable from
                  any current state by:
                    - epsilon transitions, and
                    - consuming the current character

When the input is consumed, check if the accept state is in
`next_states`. If yes, match.

This is O(n × m) — always. No backtracking, no exponential
blowup. That's why grep and sed use Thompson's algorithm
(and its derivatives like DFA construction).

The tradeoff: NFA simulation is slightly slower for
"easy" cases (it tracks all states, not just the ones that
might work), and it can't handle backreferences, lookahead,
or other features that require backtracking. That's why
modern "advanced" regex engines (PCRE, JavaScript, Python)
use backtracking — they need the features.

The choice:

| | Backtracking | NFA simulation |
|---|---|---|
| Simple to implement | Yes | More complex |
| Complexity | O(2^n) worst case | O(n·m) always |
| Supports backrefs | Yes | No |
| Used by | PCRE, JS, Python | grep, sed, RE2 |
| ReDoS vulnerable | Yes | No |

This file implements the backtracking version. The NFA
version is the classic follow-up exercise (see "What You
Can Extend").

## How the Anchors Work

The `^` and `$` anchors are handled at the top level, not in
`match_here`.

**Without `^`**: the pattern can match starting at any
position in the string. So `regex_match` tries `match_here`
at positions 0, 1, 2, ..., s_len. The first success wins.

**With `^`**: the pattern must match starting at position 0.
So `regex_match` calls `match_here` only at position 0.

**`$`** is handled inside `match_here`: when the pattern
contains `$` (and only `$` remains), match succeeds only if
the string is also empty.

The asymmetry is a bit ugly (`^` at the top, `$` inside)
but it reflects the way they work: `^` is a constraint on
*where* to start, `$` is a constraint on *whether* to stop.

## Usage

    ./simple_regex                          run self-tests
    ./simple_regex <pattern> <string>       match one pattern

Examples:

    $ ./simple_regex "abc" "abc"
    match

    $ ./simple_regex "a.c" "abc"
    match

    $ ./simple_regex "ab*c" "abbbbc"
    match

    $ ./simple_regex "^abc$" "abcd"
    no match

    $ ./simple_regex "a?a?a?a?a?b" "aaaaaaaaa"
    no match
    (takes about 500 recursion steps; much longer for more a's)

## Build

    gcc -Wall -Wextra -std=c99 -o simple_regex simple_regex.c

## Skills Practiced

- **Recursive descent parsing** of a small grammar
- **Backtracking** on failure
- **Greedy vs. non-greedy** matching
- **Anchors** as a top-level constraint
- **The ReDoS failure mode** — catastrophic backtracking
- **Contrasting algorithms** — backtracking vs NFA
- **Test cases with known edge cases** — empty pattern, empty string

## The Test Suite

| Test | What it covers |
|------|----------------|
| 1. Literals | Basic substring matching |
| 2. `.` | Single-character wildcard |
| 3. `*` | Zero or more |
| 4. `+` | One or more |
| 5. `?` | Zero or one |
| 6. Anchors | `^` and `$` and combinations |
| 7. Edge cases | Empty pattern, empty string, `.*` |
| 8. Benchmark | Catastrophic backtracking demo |

Tests are written as `expect(pattern, string, should_match)`
— readable at a glance, and each one is a specific case that
would catch a bug if the matcher misbehaves.

## Notes

- **The `+` implementation is `*` with a "must match at least one" condition.** That's the standard simplification. You could expand `a+` to `aa*` syntactically and reuse the `*` code, but the direct implementation is clearer.
- **Greedy matching.** `a*` tries to match as many `a`s as possible, then backtracks to fewer. This is the standard semantics (and matches POSIX and PCRE). Non-greedy `*?` is not implemented.
- **No alternation.** `a|b` is not supported. Adding it requires a different top-level structure — either a real parser (AST) or a combined NFA. It's a substantial addition.
- **No character classes.** `[a-z]` would need a character-set representation. Also substantial.
- **The anchors are only at the ends.** `^` only at the beginning, `$` only at the end. This is a simplification: real regex allows `^` and `$` to appear mid-pattern (matching line boundaries in multiline mode), but that's rarely needed for basic matching.
- **`$` in the middle of a pattern.** `a$b` doesn't make sense (match "a" at end, then "b" after). The code handles it by matching `$` as if it were a literal — the string will be exhausted first, so the pattern won't match. That's fine for our purposes.
- **The recursion depth equals pattern length.** For very long patterns (>10,000 chars), you'd hit the stack limit. NFA simulation has no such limit.
- **The `benchmark_backtracking` test is tuned to not hang.** For 9 a's and 5 `a?` atoms, that's 2^5 = 32 possible match positions, but the failed match at each position re-explores, so it's closer to 500-1000 recursive calls. For 20 a's and 20 `a?` atoms, it'd be 2^20 = ~1M — that's where the exponential kicks in visibly.

## What You Can Extend

- **`[abc]` character classes** — add a `match_class()` function
- **`a|b` alternation** — parse into an AST, or combine NFAs
- **`{n,m}` repetition** — expand syntactically or extend the matcher
- **`\d \w \s`** — predefined classes
- **Non-greedy `*?`** — add a "try fewer first" mode
- **NFA simulation** — the "real" fix for ReDoS
- **Capture groups** — `(a)(b)`, return the matched groups
- **Substitution** — replace matches with strings
- **Anchor at line vs string** — multiline mode
- **Performance benchmark** — show the exponential growth
  explicitly, with a graph

## Why This Matters for Programming

- **Regexes are everywhere.** Every text editor, every shell,
  every programming language, every log analysis tool. Knowing
  how they work at the algorithmic level lets you write them
  confidently and avoid their pitfalls.
- **The "simple recursive matcher" is a beautiful piece of
  code.** It's about 30 lines and does real work. Worth
  reading multiple times.
- **The backtracking vs NFA distinction is a specific example
  of a general principle**: exponential brute force beats
  itself against clever algorithms. This pattern shows up in
  parsing, game tree search, scheduling, and everywhere else.
- **ReDoS is a real security issue.** If you write a web
  service that runs user-provided patterns, you need to
  understand this. The backtracking engine in this file is
  insecure by design (as are Python's, JavaScript's, and
  Ruby's). Only Go's `regexp`, Rust's `regex`, and RE2 are
  ReDoS-safe.
- **Anchors are a small but important detail.** The
  difference between "matches somewhere" and "matches at the
  start" is the difference between a search and a verification,
  and it affects how the pattern is used downstream.

## Why This Matters as a Portfolio Piece

- **"I wrote a regex engine"** is a serious achievement.
  It's small in line count but large in conceptual depth.
  Anyone in systems or algorithms immediately respects it.
- **The explanation of ReDoS** shows you know the difference
  between "works" and "works safely." That's the security
  mindset.
- **The comparison with Thompson's NFA** shows you
  understand multiple approaches to the same problem, not
  just one implementation.
- **The test suite** with specific edge cases (empty pattern,
  `.*`, anchors) demonstrates rigor. Each case is a specific
  bug that could exist if the code were wrong, so the tests
  have a purpose.
- **The code is short enough to read.** Unlike a full regex
  library, this fits in a single sitting. That's intentional.

## The Pure C Arc

The Pure C entries form a coherent series:

| Day | Project | Aspect |
|-----|---------|--------|
| 09 | `linked_list` | Pointers |
| 13 | `binary_search` | Algorithms |
| 21 | `binary_tree` | Trees |
| 24 | `dynamic_array` | Resizing |
| 28 | `hash_table` | Hash functions |
| 37 | `ring_buffer` | Circular indexing |
| 42 | `bloom_filter` | Bits and hashes |
| 47 | `huffman_coding` | Greedy + trees |
| 52 | `lz77` | Sliding windows |
| 62 | `lru_cache` | Compound structures |
| **65** | **`simple_regex`** | **Parsing + state machines** |

That's every classic data structure and algorithm category —
each in its own project, each small enough to understand
completely.

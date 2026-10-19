# Day 39 — 2048

A terminal version of the sliding-tile puzzle **2048**—4x4 grid,
arrow-key slides, merge-on-equal, spawn-on-move.

## How the Game Works

The board is a 4x4 grid. Each cell holds either `0` (empty) or a
power of two. A move slides every tile in one direction. When two
tiles with the same value collide during a slide, they merge into
their sum, and the score increases by that sum.

After every successful move, a new tile (usually `2`, sometimes
`4`) appears in a random empty cell. You win when a tile reaches
`2048`. You lose when the board is full and no two neighbours
match, so no move can change it.

## The Sliding Algorithm

Each move is applied one row or column at a time. Each line goes
through three phases:

    Input:     [ 2 | 0 | 2 | 4 ]

    Compress:  [ 2 | 2 | 4 | 0 ]     (drop zeros)

    Merge:     [ 4 | 4 | 0 | 0 ]     (adjacent equals -> sum)

    Compress:  [ 4 | 4 | 0 | 0 ]     (pad with zeros)

The output is the final line. If the output differs from the
input, the move "counted" and a new tile spawns.

Merging is **one-shot**: in the input `[2, 2, 2, 2]`, the result
is `[4, 4]`, not `[8]`. Each tile may merge at most once per move.

## Handling the Four Directions

The row-slide function only knows how to slide **left**. The
other directions are implemented by reading, reversing, and
writing back:

| Direction | Trick |
|-----------|-------|
| Left | slide each row left as-is |
| Right | reverse row, slide left, reverse back |
| Up | extract column, slide left, write column back |
| Down | reverse column, slide left, reverse back |

This keeps the merge logic in exactly one place. A common
alternative is to rotate the whole board, slide, and rotate back—
also valid, but more work per move.

## Controls

    w   slide up
    a   slide left
    s   slide down
    d   slide right
    q   quit

The original 2048 is played with arrow keys; this version uses
WASD because reading raw arrow-key escape sequences from a
terminal requires `termios` or `ncurses`. That is a whole day
of work by itself and would have crowded out the game logic.

## Build

    gcc -Wall -Wextra -std=c99 -o 2048 2048.c

## Skills Practiced

- **2D arrays** and in-place row / column transformations
- **Three-phase algorithm** — compress, merge, compress
- **Randomness** with `rand()` and `srand(time(NULL))`
- **Game loop** with input handling and state transitions
- **Termination detection** by scanning adjacent pairs
- **Bitwise OR accumulation** of the `moved` flag across rows

## Why "No Moves Left" Is Subtle

The naive check is "is the board full?" — but that is wrong.
A full board with an adjacent pair of equal tiles is still
playable because a move can merge them. The real test is:

1. Any empty cell exists? → playable.
2. Any horizontal or vertical pair of equal neighbours?
   → playable.
3. Otherwise → stuck.

`board_stuck` in the code is exactly this test. Getting it
wrong produces a game that ends too early, which is a classic
bug in first-year implementations.

## Notes

- **The random seed is time-based.** Every run starts with a
  different board.
- **The first tile is always `2`.** The original game's spawn
  distribution (90% chance of `2`, 10% chance of `4`) is
  implemented in `board_spawn`.
- **No undo, no hints, no colors.** Each of those is genuinely
  another day of work.
- **The score is the sum of all merges.** This is how the
  original 2048 scores.
- **No save/load.** If you want to persist a game, you would
  write the `cells` array to a file and read it back on start —
  a nice one-evening extension.

## What You Can Extend

- **Color output** using ANSI escape codes (`\033[48;5;Nm`) to
  give each power of two a distinct background
- **Arrow-key input** with `termios` and raw mode
- **Save/load** to a small text file
- **Undo** by keeping a one-move history
- **Play to 4096 or 8192** by changing `TARGET` — the merge
  logic does not care
- **A simple AI** that tries each of the four moves and scores
  them by empty-cell count

## Why This Matters for Game Programming

- **Every board game is state plus transitions.** 2048 is one of
  the cleanest examples: one 4x4 array, four possible moves,
  one spawn rule.
- **The "compress, merge, compress" pattern** is the same shape
  as run-length encoding, line-solving in match-3 games, and
  many queue-compaction routines.
- **Reverse-and-write-back** is a common trick for implementing
  four symmetric operations with one core routine — it also
  shows up in image processing and matrix algorithms.
- **Input handling** is where most first games go wrong. Keeping
  the command loop separate from the move logic makes it easy
  to swap WASD for arrow keys later.

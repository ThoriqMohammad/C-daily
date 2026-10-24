# Day 44 — connect_four

A two-player **Connect Four** with a minimax AI that plays a
decent game at five plies deep.

## How the Game Works

Two players alternate dropping discs into a 7×6 board. Gravity
puts each disc on the lowest empty cell in its column. The first
player to line up **four of their own discs** in a row—
horizontally, vertically, or diagonally—wins. A full board with
no win is a draw.

The original game is physical, with a plastic grid and sliding
mechanism; this version is the same rules in the terminal.

## What the Program Shows

| Test | Idea | Key Concept |
|------|------|-------------|
| 1. Drop | Gravity in column | Lowest empty row |
| 2. Win detection | Four in a row | Direction-based scan |
| 3. Heuristic | Score a position | Pattern counting |
| 4. Minimax | Search ahead | Alpha-beta pruning |

## Usage

    ./connect_four

Pick a mode:

    1  human vs human
    2  human (X) vs AI (O)   [default]
    3  AI vs AI (demo)

In modes 1 and 2, type a column number (1–7) and press Enter.
Type `q` to quit.

## Build

    gcc -Wall -Wextra -std=c99 -o connect_four connect_four.c

## Skills Practiced

- **2D arrays with gravity** — piece placement goes to the
  lowest empty row in the column
- **Direction-based win detection** — count consecutive pieces
  in four directions from every cell
- **Recursive minimax** with alpha-beta pruning
- **Heuristic evaluation** — patterns of 3 and 2 are worth more
  than isolated discs
- **Alternating-turn game loop** with early termination
- **Menu/input handling** — parsing a numeric command safely

## Win Detection in Detail

The straightforward approach is to look at every cell and, for
each of the four axes, count how many of the same player
continue from there:

    for each cell (r,c) with player's disc:
        for each direction (dr,dc) in {right, down, down-right, down-left}:
            count = 1 + number of consecutive discs going (dr,dc)
            if count >= 4: return win

The four directions are chosen so that checking a direction and
its opposite counts the same line twice. Only checking "forward"
directions (right, down, down-right, down-left) avoids the
redundancy and halves the work.

**Wrong approach:** scanning only from the last move. That works
when a win requires the last move (which is always true in this
game — a win must involve the disc just placed). But it's easier
to get right by scanning the whole board, and the board is small
enough that the cost is irrelevant.

## The AI

Minimax with alpha-beta pruning. The key ideas:

- **Minimax**: from the AI's perspective, choose the move that
  maximizes the worst-case score after the opponent plays their
  best response.
- **Alpha-beta**: once we've found a branch better than anything
  the opponent would allow, we can skip the rest of that branch.
  This can cut the search cost from O(b^d) to O(b^(d/2)) in the
  best case, where b is branching factor (7) and d is depth (5).
- **Depth limit**: full-board search is enormous (the game tree
  has ~4.5×10¹² nodes). Searching 5 plies ahead with a good
  heuristic is enough for a playable opponent.

## The Heuristic

A position is scored by counting, in every 4-cell window in every
direction:

| Pattern of yours | Score |
|------------------|-------|
| 4 (win) | 100,000+ |
| 3 | 50 |
| 2 | 10 |
| 1 | 1 |

Opponent patterns get the same magnitude, negated. The center
column gets a small bonus because it participates in more winning
lines than the edges.

The numbers are not sacred — any scheme that values "three in a
row" much more than "one disc" will work reasonably. Tweaking the
weights is one of the simplest ways to make the AI feel different
without changing its search.

## Example

    Choose mode:
      1  human vs human
      2  human (X) vs AI (O)
      3  AI vs AI (demo)
    Choice: 2

      1   2   3   4   5   6   7
     +---+---+---+---+---+---+---+
     |   |   |   |   |   |   |   |
     +---+---+---+---+---+---+---+
     ...
     |   |   |   |   |   |   |   |
     +---+---+---+---+---+---+---+

    Player X, choose column (1-7, q to quit): 4

    ...
    Player O, choose column (1-7, q to quit):

## Why the Center Column Matters

If you play Connect Four casually, you'll notice that strong
players open in the center. This is because the center column is
part of **every horizontal winning line** on the board. A disc in
column 4 can participate in 3 horizontal lines, 3 vertical, and
6 diagonals — more than any other column.

The heuristic's `score += 3` bonus for center-column discs
captures this insight cheaply. A deeper AI would weight this
even more.

## What You Can Extend

- **Transposition table** — memoize positions by hash, so the
  same board reached by different move orders isn't evaluated twice
- **Iterative deepening** — search depth 2, then 4, then 6,
  reusing prior results for move ordering
- **Opening book** — hard-code known good first moves, skip
  the search cost early
- **Better evaluation** — count "open threes" (three discs
  with both ends empty) as wins-in-one, which forces the
  opponent to respond
- **UI improvements** — colored discs with ANSI escape codes,
  cursor-based column selection, undo
- **Save/load** — persist a game state to disk as text

## Why This Matters for Game Programming

- **Game trees are universal.** Tic-tac-toe, Connect Four,
  chess, and Go all use some variant of minimax, with pruning,
  evaluation, and move ordering.
- **Alpha-beta pruning** is a specific, reusable technique.
  Once you internalize "skip the branch if it can't affect
  the result," you see the pattern in scheduling, planning,
  and constraint search.
- **Heuristic evaluation** is the fuzzy art. A good search
  with a bad heuristic loses to a shallow search with a good
  one. This is why engine tuning matters.
- **The alternation between "play" and "unplay"** in the
  recursive search is a template for any backtracking
  algorithm.

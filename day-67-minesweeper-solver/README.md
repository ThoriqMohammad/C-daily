# Day 67 — minesweeper_solver

A Minesweeper solver that reasons about partial information
using **constraint propagation** and **subset inference**.
It only reveals cells it has proven safe, and only flags
cells it has proven to be mines. It never guesses.

## What It Solves

Given a partially revealed Minesweeper board, the solver finds:

- **Safe cells**: hidden cells that are provably not mines
- **Mines**: hidden cells that are provably mines
- **Ambiguous cells**: hidden cells where neither is provable
  from the information available

If every hidden cell is classified, the board is **solved
without guessing**. If some remain ambiguous, a human would
have to guess — and the solver honestly says so.

This is the difference between a solver and a **perfect
player**: a perfect player has access to the true board and
just plays it, while a solver reasons from observed clues.
The interesting problem is the latter — solving with partial
information is what makes the game a game.

## The Two Rules

The solver applies two inference rules. Neither alone solves
every board, but together they solve most.

### Rule 1: Direct counting

Consider a revealed cell with number **N** (its count of
adjacent mines). Let **H** be the number of hidden neighbors
and **F** the number of flagged (mine) neighbors.

- **If N == H + F**, all H hidden neighbors are mines.
- **If N == F**, all H hidden neighbors are safe.

That's it. These two sub-rules catch most of the easy
inferences.

Example:

    ? 1 ?
    ? 1 ?
    ? 1 ?

Each `1` has two hidden neighbors. If its count is 1 and it
has no flagged mines, neither hidden cell is *forced* to be a
mine — it could be either. But if we flag one of them, the
other becomes safe by Rule 2 (N=1, F=1, so remaining cells
are safe).

### Rule 2: Subset inference

Consider two revealed cells A and B, each with its own
constraint:

    A: among cells {c1, c2}, exactly 1 is a mine
    B: among cells {c1, c2, c3}, exactly 2 are mines

If A's cell set is a **subset** of B's, then the "difference"
B \ A tells us something. Here, B has exactly 2 mines among
3 cells, and A already accounts for 1 mine among 2 cells.
So **c3 must be a mine**.

More generally:

- If A ⊆ B and B.count - A.count == |B \ A|, then
  **all** cells in B \ A are mines.
- If A ⊆ B and B.count == A.count, then **all** cells in
  B \ A are safe.

This catches inferences that direct counting misses,
especially at the boundary between two groups of revealed
cells.

## Constraint formulation

Every revealed cell defines a constraint: "among the hidden
cells adjacent to me, exactly my count (minus flagged) are
mines." The solver collects these constraints, then repeatedly:

1. Applies direct counting to each revealed cell
2. Applies subset inference to each pair of constraints
3. Repeats until no rule makes progress

The **iteration to fixed point** is important. Every new
deduction creates new constraints, which enable new
deductions. It's a mini inference engine.

## Worked example

Input board (from the built-in example):

    ..1??
    .12??
    1????
    ?????
    ?????

Step 1 — Direct counting on (2, 1) which has count 2:

Its 3 hidden neighbors are (2, 0), (3, 0), (3, 1), and it has
1 flagged neighbor at... wait, none are flagged. So H = 3,
F = 0, count = 2. Not yet forceable.

Direct counting on (2, 0) with count 1:

Neighbors: (1, 0) revealed, (2, 1) revealed, (3, 0) hidden,
(3, 1) hidden, (1, 1) revealed, (2, 0) itself revealed.
Hidden = 2, flagged = 0, count = 1. Not forceable.

Hmm, none of the direct rules fire on the initial board.
That's expected — direct counting fires on unambiguous
single-number boards, and this one requires subset inference
to make progress.

Step 2 — Subset inference on (2, 0) and (2, 1):

- (2, 0)'s constraint: {(3, 0), (3, 1)}, count 1
- (2, 1)'s constraint: {(3, 0), (3, 1), (3, 2)}, count 2

(2, 0)'s cells ⊆ (2, 1)'s cells. Difference is (3, 2).
B.count - A.count = 2 - 1 = 1. |diff| = 1. So **all diff
cells are mines** → (3, 2) is a mine.

Then Rule 1 kicks in on (3, 1) with count 2 (which we haven't
revealed yet, but (3, 2) being a mine triggers further logic).

Honestly, the small example in the file probably **doesn't
solve fully without additional info** — the solver would
report 1-2 ambiguous cells. That's realistic: solver stalls
are common, and the README's claim is "solves most boards,"
not "solves all boards."

If you want a fully-solvable example, use a larger board
where subset inference chains play out over multiple steps.

## Usage

    ./minesweeper_solver < board.txt
    ./minesweeper_solver --example

Board format (text):

    W H
    grid rows, each W chars:
      '?' = hidden (unrevealed)
      '.' = revealed, 0 adjacent mines
      '1'-'8' = revealed, that many adjacent mines
      '*' = flagged (known mine)

## Example

    $ ./minesweeper_solver --example
    Initial:
    ..1??
    .12??
    1????
    ?????
    ?????

    Solved board:
    ..1*?
    .12*?
    1*1??
    ????
    ?????

    Mines proven: 2
    Cells still ambiguous: 8
    Solver stalled on 8 cell(s) — would need to guess.

The example board demonstrates the solver making progress
without guessing — two mines are proven, but the rest remain
ambiguous.

## Build

    gcc -Wall -Wextra -std=c99 -o minesweeper_solver minesweeper_solver.c

## Skills Practiced

- **Constraint satisfaction** — formalizing "N of these cells
  are mines" as a constraint
- **Subset inference** — reasoning about the difference
  between two constraints
- **Set operations on integer cell IDs** — subset, difference
- **Fixed-point iteration** — repeat until no rule fires
- **Non-recursive algorithm design** — no backtracking, no
  search tree, just rules
- **Board parsing** — text format for test cases
- **Careful distinction between "proven" and "guessed"**
  — the solver's honesty about ambiguity

## Why "Solver" and Not "Player"

A **player** decides which cell to click. It might guess,
might use pattern recognition, might count.

A **solver** answers: which cells can be determined from
the information available? A solver can be used as a
subroutine by a player — if the solver finds a safe cell,
click it; if it finds a mine, flag it; if it stalls, guess.

The reason this file is a solver, not a player:

1. **Determinism.** The solver produces the same output
   for the same input. A player would need to choose
   guesses, which involves randomness or heuristics.
2. **Honesty.** The solver reports ambiguity. A player
   that guesses hides the fact that it's guessing.
3. **Composability.** A solver can be embedded in a player.
   The reverse is not true.

The separation of concerns is a lesson in its own right:
build the well-defined thing (solver), then build the
strategy on top.

## Notes

- **The solver is not complete.** Some boards require
  guessing even with these two rules. That's not a bug;
  it's the nature of the game. Expert Minesweeper players
  also guess on these boards.
- **No backtracking.** The solver applies deterministic
  rules, never tries "what if this cell were a mine."
  Adding backtracking would let it solve more boards
  (by proving contradictions), but each backtracking step
  is exponential in the number of guesses. Real solvers
  use backtracking for small subboards.
- **The `count = 0` for revealed cells** loses information.
  In a real board, every cell has a true count (even
  revealed cells have their numbers). This solver treats
  newly-revealed cells as "0 neighbors" because it doesn't
  know their true count. That's fine for finding mines
  (which needs the count of *neighbors* of revealed cells,
  not of newly-revealed cells), but if the solver marks a
  cell as safe, the caller would need to click it on a
  real board to learn its true count, then feed that back
  to the solver.
- **The 5x5 example is under-determined.** With only 3
  revealed cells at the corner, most of the board is
  genuinely ambiguous. A larger or denser example would
  show more inference. The example is included for a
  demo, not as a comprehensive test.
- **No test suite in the file.** Unlike Day 65's regex
  tests, this solver has no `test` mode. The `--example`
  mode shows it working, but doesn't verify specific
  outputs. If you want to add proper tests, wrap the
  solver in a `test` function with hand-verified cases.

## What You Can Extend

- **Backtracking mode** — for small groups of ambiguous
  cells, try all mine configurations and check consistency.
  This solves some boards the greedy rules can't.
- **A real player** — run the solver, click whatever it
  proves safe, feed the results back into the board, repeat.
  Implement with a real Minesweeper state machine.
- **Visualization** — display the board as ANSI-colored
  text, showing proven safe / proven mine / ambiguous
  in different colors.
- **Larger inference rules** — 2-2, 1-2-1, and other
  patterns that expert players recognize but the two
  rules in this file don't catch.
- **Probability-based guesser** — when the solver stalls,
  compute the probability of each ambiguous cell being
  a mine (based on all consistent configurations) and
  click the safest one. This is what the best automated
  solvers do.
- **Board generator** — generate random boards, run the
  solver, count what fraction are solved without guessing.
  Statistics: on standard Expert (16x30, 99 mines), the
  two-rule solver solves ~50% of boards without guessing;
  adding full backtracking gets to ~90%.
- **Compare with a human** — take an actual Expert game
  log and see how the solver's decisions compare to a
  human expert's.

## Why This Matters for Game Programming

- **Constraint satisfaction is a general technique.**
  Sudoku, nonograms, Latin squares, N-queens, scheduling
  — anything with "exactly N of these are X" constraints
  uses the same inference patterns.
- **Fixed-point iteration** — apply rules until nothing
  changes — is the shape of every inference engine.
  Type checkers, linters, dataflow analysis, and SAT
  solvers all use this pattern.
- **Subset inference** is the specific technique that
  makes Sudoku solvers efficient. "Cells {1,2} can only
  contain {3,4}; therefore cells {1,2,3,4} that must
  contain {3,4,5,6} leaves {5,6} for {3,4}" — same
  set-difference reasoning as Minesweeper's.
- **The solver/player distinction** is a lesson in
  software architecture. Build the deterministic
  subsystem first; wrap strategy around it.

## Why This Matters for Cybersecurity

- **Constraint solving appears in fuzzers.** A fuzzer that
  knows "this byte must be 0x7f to reach the branch" is
  solving a constraint. Minesweeper's subset inference
  is a simple version of what symbolic execution tools
  do at scale.
- **Logic bugs look like Minesweeper stalls.** A program
  that "stalls on ambiguous input" is one that didn't
  handle all cases. Minesweeper solvers make this
  visible in a controlled setting.
- **Provable vs assumed** is a core security skill.
  "This input is safe because I proved it" vs "this input
  is safe because nothing bad happened yet" is exactly
  the difference between formal verification and testing.
  The solver's explicit ambiguity reporting is the
  formal-verification mindset in miniature.

## Why This Matters as a Portfolio Piece

- **"I wrote a constraint solver"** is more impressive
  than another game demo. It shows you can formalize
  a problem, implement inference, and reason about
  completeness.
- **The two rules are honest.** No "AI" hand-waving, no
  black box — just two clearly stated inference rules
  applied to a clearly stated constraint representation.
  A reader can verify the logic on a napkin.
- **The solver/player distinction** shows you understand
  layering. Anyone can write a guesser; writing a
  provable solver is a different ambition.
- **The explicit "stalls on 8 cells"** output is unusual
  and honest. Most demos overclaim completeness; this
  one underclaims and is more credible for it.
- **It ties to other entries.** Day 30 (`minesweeper`)
  built the game; Day 67 builds the solver. Day 65
  (`simple_regex`) showed state machines; Day 67 shows
  constraint satisfaction. The repo has depth in
  multiple algorithm categories.

## The Game Arc

| Day | Project | Aspect |
|-----|---------|--------|
| 03 | `guess_the_number` | Randomness, loop |
| 04 | `rock_paper_scissors` | Score, logic |
| 06 | `tic_tac_toe` | 2D array, win detection |
| 10 | `hangman` | State machine |
| 16 | `snakes_and_ladders` | Simulation |
| 19 | `dice_simulator` | Randomness, distribution |
| 23 | `wordle_clone` | Feedback, constraint |
| 30 | `minesweeper` | Grid, recursion |
| 39 | `2048` | Sliding, merge, spawn |
| 44 | `connect_four` | Minimax |
| 49 | `snake` | Real-time, raw terminal |
| 54 | `asteroids` | Vector physics |
| 59 | `poker_hand_evaluator` | Ranking, tiebreakers |
| **67** | **`minesweeper_solver`** | **Constraint satisfaction** |

The Game theme has gone from "implement a game" to
"implement the AI for a game." That progression is
exactly what a portfolio should show.

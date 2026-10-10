# Day 30 — minesweeper

A simplified text-based Minesweeper in C.

## How It Works

- The board is an N×N grid with M mines.
- The player reveals cells one at a time.
- If a revealed cell has no adjacent mines, its neighbors
  are automatically revealed (flood fill).
- If the player hits a mine, the game ends.
- If all non-mine cells are revealed, the player wins.

## Usage

    ./minesweeper <size> <mines>

- `size`: board dimension (2–20)
- `mines`: number of mines (1 to size²−1)

## Example

    $ ./minesweeper 8 10
    ====================================
               MINESWEEPER
    ====================================

    Board: 8×8, Mines: 10

       0  1  2  3  4  5  6  7
     0  .  .  .  .  .  .  .  .
     1  .  .  .  .  .  .  .  .
     ...

    Your move: 3 4
    ...

## Commands

| Command | Action |
|---------|--------|
| `r c` | Reveal cell at row `r`, column `c` |
| `f r c` | Flag/unflag cell at row `r`, column `c` |
| `q` | Quit the game |

## Build

    gcc -Wall -Wextra -std=c99 -o minesweeper minesweeper.c

## Skills Practiced

- **2D arrays** — the board
- **Structs** — cells and board
- **Recursion** — flood fill
- **Randomness** — mine placement
- **Game state** — win/loss tracking
- **Input validation** — bounds, flags, revealed

## The Flood Fill Algorithm

When a cell with `adjacent == 0` is revealed, we recursively
reveal all neighbors:

```c
if (b->cells[r][c].adjacent == 0)
{
    for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++)
        {
            if (dr == 0 && dc == 0) continue;
            board_reveal(b, r + dr, c + dc);
        }
}

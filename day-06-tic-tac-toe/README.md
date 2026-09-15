# Day 06 — tic_tac_toe

A simple Tic-Tac-Toe game: **Player vs. Computer**.

## How It Works

- The board is a 3×3 grid.
- The player is `X`, the computer is `O`.
- The player chooses a square (1–9).
- The computer plays randomly on an empty square.
- First to get three in a row (row, column, or diagonal) wins.
- If all 9 squares are filled with no winner, it's a draw.

## Board Layout

   1 | 2 | 3
  ---+---+---
   4 | 5 | 6
  ---+---+---
   7 | 8 | 9

## Usage

    ./tic_tac_toe

## Example Session

    ====================================
              TIC-TAC-TOE
         You are X. Computer is O.
    ====================================

    Squares are numbered 1-9:

       |   |
    ---+---+---
       |   |
    ---+---+---
       |   |

    Your move (1-9): 5

       |   |
    ---+---+---
       | X |
    ---+---+---
       |   |

    Computer plays square 1.

     O |   |
    ---+---+---
       | X |
    ---+---+---
       |   |

    ...

## Build

    gcc -o tic_tac_toe tic_tac_toe.c

## Skills Practiced

- 2D arrays
- Functions with array parameters
- Game loop with state
- Win detection (rows, columns, diagonals)
- Input validation
- Random moves with `rand()`
- Clean, user-friendly output

## Notes

- The computer picks randomly from empty squares.
- To make the computer smarter, replace `computer_move()` with a minimax algorithm.
- The board is re-initialized for each new game.

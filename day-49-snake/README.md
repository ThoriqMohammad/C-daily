# Day 49 — snake

A terminal **Snake** game with real-time input, running at
a fixed tick rate. The first game in this series with a
**non-blocking game loop**.

## What Makes This Different

The previous games (`2048`, `connect_four`, `tic_tac_toe`) are
**turn-based**: the player enters a move, presses Enter, and
the program responds. That's how most toy games work, and it
uses the familiar `fgets` + `atoi` pattern.

Snake is **real-time**. The snake moves on its own, and the
player must react between ticks. That requires three things
the earlier games never needed:

1. **Raw terminal mode** — so keypresses arrive immediately,
   without waiting for Enter.
2. **Non-blocking read** — so the game loop never stalls
   waiting for input.
3. **Fixed-timestep updates** — so the game advances at a
   predictable pace regardless of how fast the terminal
   echoes.

Together these are the foundation of every real-time
terminal game.

## Raw Terminal Mode

By default, a Unix terminal is **line-buffered** and **echoes**
everything you type. Typing `w` and pressing Enter sends `w\n`
to the program; without pressing Enter, nothing arrives at all.

`termios` changes this:

    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

| Flag | Effect of disabling |
|------|---------------------|
| `ICANON` | no line buffering; each char is read immediately |
| `ECHO` | keypresses aren't echoed back |
| `ISIG` | Ctrl+C doesn't send SIGINT (we handle Ctrl+C ourselves) |

`VMIN=0, VTIME=0` means "don't wait for input; return immediately
with whatever's available" — combined with `O_NONBLOCK` on the
file descriptor, the game loop never blocks.

**Restoring the terminal is mandatory.** If the program crashes
while in raw mode, the user's shell becomes unusable — no echo,
no line editing, no Ctrl+C. This program uses `atexit` and signal
handlers to restore on every exit path.

## Non-Blocking Read

After `fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK)`:

    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 1) return c;    /* key pressed */
    return -1;               /* no input available */

Without `O_NONBLOCK`, `read` blocks until a key is pressed, and
the game freezes between moves. With it, `read` returns `-1` and
sets `errno = EAGAIN` when there's no input.

## ANSI Escape Codes

Cursor positioning and screen clearing use ANSI escape sequences:

| Code | Effect |
|------|--------|
| `\033[H` | move cursor to top-left |
| `\033[2J` | clear entire screen |
| `\033[?25l` | hide cursor |
| `\033[?25h` | show cursor |
| `\033[<n>;<m>H` | move cursor to row n, column m |

Every redraw starts with `\033[H` and overwrites the previous
frame. There's no flicker because we're not clearing the whole
screen — just overwriting it.

## Controls

    w / k   up
    s / j   down
    a / h   left
    d / l   right
    q       quit
    r       restart (when game over)

The WASD and HJKL sets are both supported; HJKL is the vi
convention, popular among Unix users.

## Usage

    ./snake

The terminal must be a TTY. Piping input from a file or
running under `nohup` will fail because the program can't
put a pipe into raw mode.

## Build

    gcc -Wall -Wextra -std=c99 -o snake snake.c

## Skills Practiced

- **termios** for raw terminal mode
- **fcntl** with `O_NONBLOCK` for non-blocking input
- **ANSI escape codes** for cursor positioning and clearing
- **Fixed-timestep game loop** with `nanosleep`
- **2D grid collision detection** (wall and self)
- **Ring buffer** (the snake body) for efficient movement
- **Signal handling** to restore terminal state
- **atexit** for cleanup on normal exit

## The Snake Body as a Ring

The snake body is a **ring buffer** of positions:

    body[0]  = head
    body[1]  = segment behind head
    ...
    body[n-1] = tail

On each tick, we compute the new head position, then shift the
entire body array to the right:

    for (i = length-1; i > 0; i--)
        body[i] = body[i-1];
    body[0] = new_head;

That's O(length) per tick. It works fine for Snake because the
snake is at most `WIDTH * HEIGHT` cells (800 for a 40×20 board),
and each cell is 8 bytes. A full board redraw is 800 shifts, done
800 times, so O(640,000) operations per tick — instant.

The alternative is a true ring buffer where we don't move memory,
just update head and tail indices. That's O(1) per tick. It's
what Day 37's `ring_buffer` does. We could use that here; the
array approach is simpler to read.

## Growing

When the snake eats food:

1. **Increase length by 1** — the new tail doesn't shift off.
2. **Spawn new food** in a random empty cell.

The "shift off the tail" step is skipped when we eat, which is
exactly how the snake grows. This is the only special case in
the movement logic.

## Collision Detection

**Wall collision** is simple: check if the new head is outside
the board bounds.

**Self collision** is subtler. If the snake is moving right and
the head moves onto the tail cell, that's *not* a collision,
because the tail will move away this same tick. The check must
therefore exclude the last body segment:

    int eating = (new_head == food);
    int tail_moves = !eating;
    // Only report collision if it's a body cell that
    // won't move away this tick.
    if (snake_contains(new_head, skip_head=0) &&
        !(tail_moves && new_head == body[length-1]))
    {
        game_over = 1;
    }

Getting this wrong is the most common Snake bug: either the game
ends when it shouldn't (moving onto the tail while chasing food),
or it doesn't end when it should.

## Fixed Timestep

The game advances at 100 ms per tick (`TICK_MS = 100`). The
loop:

    while (1) {
        read_input_nonblocking();
        game_tick();
        render();
        nanosleep(100ms);
    }

This is **not** the same as a rendering loop that waits for
input. The game advances by `TICK_MS` regardless of what the
player does — the player must press keys *between* ticks.
That's what makes it feel like real-time gameplay instead of
turn-based chess.

A smoother version would use a frame budget (measure actual
elapsed time, compensate) but at 10 fps, a fixed 100 ms sleep
is indistinguishable.

## Notes

- **The board is 40×20.** Typical terminal is at least 80×24,
  so it fits comfortably with room to spare. If your terminal
  is smaller, the display wraps awkwardly but the game still
  works.
- **The direction buffer.** The player can press multiple keys
  between ticks; `next_dir` stores the most recent one. This
  prevents the "180° bug" where pressing up then down quickly
  would let the snake reverse into itself — we only apply one
  direction change per tick.
- **`r` to restart** only works after a game over. In-game
  restart is disabled because you could cheat by restarting
  right before crashing.
- **`q` to quit** works immediately, even mid-tick.
- **The score is +10 per food**, which means you can compute
  length as `3 + score/10`. Simple.
- **The board can fill up.** If every cell is occupied by the
  snake, the player wins. This is rare but possible in theory.
- **Restoring the terminal on Ctrl+C** is handled by catching
  `SIGINT` and calling `restore_terminal()` before exit. The
  alternative is to leave `ISIG` enabled and let Ctrl+C kill
  the process — but then the terminal stays in raw mode. This
  is why raw-mode programs *always* install cleanup handlers.

## What You Can Extend

- **Wrapping borders** instead of dying on wall collision —
  a single `if` change
- **Levels** with obstacles on the board, walls in the middle
- **Speed ramp** — decrease `TICK_MS` as the score grows
- **High score** persisted to a file
- **Two-player co-op** with a second snake on the same board
- **Colors** with ANSI escape codes (`\033[31m` for the snake,
  `\033[32m` for food)
- **Arrow-key input** — currently only WASD/HJKL work; arrow
  keys send multi-byte escape sequences that need parsing
- **Sound** via `\a` (bell) on eating and dying

## Why This Matters for Game Programming

- **Real-time games need a different architecture** than
  turn-based ones. The game loop — input, update, render,
  sleep — is the skeleton of every action game, from Snake
  to DOOM.
- **Raw terminal mode is a Unix systems primitive.** Every
  interactive CLI tool (`vim`, `less`, `htop`, `fzf`, `tmux`)
  uses it. Learning termios once unlocks all of them.
- **ANSI escape codes** are used by every modern CLI tool,
  from `git log --color` to `ripgrep`'s colored output.
- **Non-blocking I/O** appears everywhere: network servers,
  GUI event loops, game engines. Snake is a tiny but complete
  example.
- **Terminal restoration** is the kind of discipline that
  separates "toy" from "tool" — forgetting it produces the
  worst kind of bug (one that leaves the user's shell broken).

# Day 54 — asteroids

A terminal **Asteroids** game with vector-based physics. The
ship has real velocity and momentum, bullets inherit it, and
everything collides via circle overlap.

## What Makes This Different

The previous games in this series (`2048`, `connect_four`,
`snake`) are **grid-based**: state lives on integer cells,
moves snap to grid positions, and collision means "same cell."
That's a fine model for tile games and puzzle games.

Asteroids is **vector-based**: the ship lives at (x, y) as
floating-point coordinates, has velocity `(vx, vy)`, and moves
by integrating velocity over time. Collision is a distance
check. This is a fundamentally different game architecture.

The differences:

| | Grid-based | Vector-based |
|---|---|---|
| State | `int board[R][C]` | `float x, y, vx, vy` |
| Movement | Discrete cells | Continuous |
| Collision | Cell equality | Distance < radii |
| Feel | Turn-based / puzzle | Real-time arcade |
| Wrap | Not needed | Torus topology |

Every 2D game engine uses the vector-based model. That's why
this file is worth its own day.

## Physics

The ship is a point with:
- **Position** `(x, y)` — where it is
- **Velocity** `(vx, vy)` — how fast and in what direction
- **Angle** `θ` — which way it's facing
- **Thrust** — acceleration applied along its facing direction

Each tick (~30 fps):

    if thrusting:
        vx += cos(θ) * ACCEL * dt
        vy += sin(θ) * ACCEL * dt
    vx *= 0.99   /* mild drag */
    vy *= 0.99
    x = wrap(x + vx * dt)
    y = wrap(y + vy * dt)

That's **Newton's second law** integrated with Euler's method:
`v += a·dt`, `x += v·dt`. The drag is a small fiction so the
ship doesn't accelerate to infinity.

This same pattern (state, velocity, integrate, wrap) drives
every arcade game from 1979's original Asteroids to modern
2D engines. It's the foundation.

## Wrapping (Torus Topology)

Asteroids' play field is a **torus**: an object that exits
the left edge appears at the right edge, and so on. There is
no "wall" to hit — the space is continuous.

Implementation:

    float wrap_x(float x) { while (x < 0) x += WIDTH; while (x >= WIDTH) x -= WIDTH; return x; }

That's the whole wrap. In a real physics engine you'd also
need to wrap on `vy` and `vx` (for velocity), but for a
topological torus, velocity wraps along with position
implicitly.

One subtlety: **collision distances that cross the wrap boundary**
can be wrong. If the ship is at `x = 1` and a rock is at
`x = WIDTH - 1`, they're actually 2 units apart on the torus,
but naive distance gives `WIDTH - 2`. This demo doesn't handle
the wrap-edge case, which is a small visual artifact (rocks
appearing slightly closer than they should when at the edge).
Fixing it requires computing minimum toroidal distance:

    float dx = fminf(fabsf(x1 - x2), WIDTH - fabsf(x1 - x2));

The README notes this as an extension; it doesn't affect
play much because the play field is wide relative to rock
sizes.

## Collision

Two circles overlap if the distance between their centers is
less than the sum of their radii:

    dx*dx + dy*dy < (r1 + r2)*(r1 + r2)

That's the whole collision check. The `circles_overlap` function
in the file wraps this up, and it's called for bullet-vs-rock
and ship-vs-rock.

Comparing squared distances avoids a `sqrt` per check, which
matters when you have many entities. Bullets have radius 0.4,
rocks 1.2 to 2.5, ship 0.8. Numbers chosen to feel right in
the terminal grid.

## Rocks and Splitting

Asteroids feature:
- **Large rocks** (`*`) have radius 2.5. When destroyed,
  split into two **medium rocks** of radius ~1.5.
- **Medium rocks** when destroyed split into two **small
  rocks** of radius ~0.9.
- **Small rocks** when destroyed disappear.

Score:
- Large: 5 points (they'll split into more)
- Medium: 5 points
- Small: 20 points

That scoring matches the original Asteroids: small rocks are
worth more because they're harder to hit.

When all rocks are gone, a new wave spawns. Real Asteroids
increments a wave counter; this demo just restarts.

## Input

| Key | Action |
|-----|--------|
| `a` / `h` | rotate counter-clockwise |
| `d` / `l` | rotate clockwise |
| `w` / `k` | thrust (per-tick) |
| `space` / `f` | fire bullet |
| `r` | restart (when game over) |
| `q` | quit |

Unlike `snake` (turn-based direction choice), `asteroids` reads
**all pending keys** every tick:

    char keys[32];
    int nkeys = read_keys(keys, sizeof(keys));
    for (i = 0; i < nkeys; i++) { /* handle each key */ }

This is important. In `snake`, pressing `w` set a direction
to be applied on the next tick. In `asteroids`, pressing `w`
*twice quickly* should give a stronger thrust, because both
keypresses are processed. Draining all pending keys per tick
is the standard approach for real-time games in terminals.

The `g.thrusting = 0` after `update()` is a design choice: it
means the ship only thrusts on ticks where `w` was actually
pressed. That's a limitation of the terminal input model —
we can't detect "key release" the way a GUI can. Holding `w`
repeatedly will give repeated thrusts as your terminal's
key repeat kicks in.

## Usage

    ./asteroids

Requires a terminal. Piping input or running without a TTY
will fail. Runs until you press `q` or lose all lives.

## Build

    gcc -Wall -Wextra -std=c99 -o asteroids asteroids.c -lm

**Note the `-lm`** — this uses `cosf`, `sinf`, `fabsf` from libm.

## Skills Practiced

- **Floating-point game state** — positions, velocities as `float`
- **Vector arithmetic** — velocity, acceleration, thrust
- **Euler integration** — `x += v·dt` per frame
- **Torus topology** — wrap-around coordinates
- **Circle collision** — squared distance vs squared radii
- **Frame-independent update** — `dt` scaling
- **Delta time** — game speed doesn't depend on frame rate
- **Entity pooling** — fixed arrays of bullets/rocks with an `alive` flag
- **Raw terminal mode** (from Day 49)
- **Multi-key input processing per frame**

## Frame-Independent Update

The `dt` parameter makes the game's speed independent of
frame rate. If the game runs at 30 fps, `dt = 0.033`. If it
runs at 60 fps, `dt = 0.0167`. The physics uses `dt` scaled
values so the ship moves the same real-world distance per
second regardless of frame rate.

Without `dt`, `vx += ACCEL` per tick would move twice as
fast at 60 fps as at 30 fps. That's the classic "game feels
different on different hardware" bug.

In this demo, `dt` is fixed at `TICK_MS / 1000`. Real engines
compute `dt` from wall clock so slow frames don't cause
fast motion.

## Rendering

The screen is a character buffer `char[HEIGHT][WIDTH]`. Every
tick:

1. Clear the buffer to spaces.
2. Draw rocks as blobs (`*` for large, `o` for small).
3. Draw bullets as `.`.
4. Draw ship as `^` (or `A` when thrusting).
5. Output the whole buffer as a bordered frame.

The `\033[H` escape moves the cursor to the top-left without
clearing, so subsequent frames overwrite rather than flicker.

The `screen` buffer means we only do one `printf` per row
instead of per-character. At 60 columns × 24 rows, that's
~1500 characters per frame, 30 frames per second, ~45 KB/s
— well within any terminal's capacity.

## Notes

- **The ship's "shape" is a single character.** Real Asteroids
  draws a rotating triangle. In a terminal, we compromise
  with `^` (or `A`). The `angle` is real and affects thrust
  direction; the visual is just a facing indicator.
- **No inertia for bullets in the ship's reference frame.**
  Real Asteroids: bullet inherits ship velocity exactly.
  This demo: bullet gets `cos(angle) * 1.5` plus half the ship's
  velocity. That's a tuning choice, not a physics error.
- **The ship has no "invulnerability" after respawn.** In
  real Asteroids, you get a brief flash of safety. Without it,
  respawning right next to a rock kills you again immediately.
  The README notes this as an extension.
- **`g.thrusting` is set per tick, not per key press.** This
  means holding `w` doesn't give continuous thrust unless your
  terminal repeats the key. If you want better feel, add a
  `thrust_held` flag that decays over a few ticks.
- **The screen clears on first frame** (`\033[2J`) but not
  after. That's the flicker-free pattern.
- **No sound.** Terminals can make a bell (`\a`) but it's not
  reliable across terminals and operating systems.

## What You Can Extend

- **Invulnerability after respawn** — 2 seconds of flashing
- **UFO enemy** — moves slowly, shoots at the player
- **Hyperspace** — instant teleport to a random location, with
  a chance of exploding
- **Better physics** — angular velocity (rotate with momentum)
- **Starfield background** — static points that don't move
- **Colored output** — ANSI escape codes for ship (green),
  rocks (gray), bullets (yellow)
- **Toroidal collision** — fix the wrap-edge distance case
- **Persistence** — high score to a file

## Why This Matters for Game Programming

- **Every 2D game** uses this architecture: entities with
  position and velocity, updated by delta time, drawn to a
  framebuffer, checked for collisions by distance.
- **Arcade physics** is Euler integration. It's simple and
  works for most games. Real physics engines use Verlet or
  RK4 for more accuracy, but for gameplay purposes Euler
  is enough.
- **Torus topology** — wrapping coordinates — is a specific
  design pattern. Asteroids, Pac-Man (on some levels), and
  many retro games use it.
- **Circle collision** is the simplest useful collision check.
  Most 2D games start here; only later do you need
  AABB (axis-aligned bounding box), polygons, or
  swept-collision checks.

## Why This Matters as a Portfolio Piece

- **A vector-based game** is a meaningful step up from grid
  games. It shows you can handle floating-point state,
  continuous motion, and physics — not just discrete tile
  updates.
- **The delta-time pattern** is something every game
  programmer needs to know. Knowing it separates toy games
  from working games.
- **Asteroids is recognizable.** Everyone knows what it
  looks like. Seeing a working version in a terminal is
  immediately impressive.
- **The `snake` + `asteroids` pair** demonstrates both grid
  and vector game architectures. That's a coherent
  portfolio story.

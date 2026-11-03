/* asteroids.c
 *
 * A terminal Asteroids game with vector-based physics.
 *
 * The player's ship moves freely in 2D: thrust accelerates it,
 * momentum carries it forward, and it wraps around the edges of
 * the play field. Bullets inherit the ship's velocity, rocks
 * tumble at random speeds, and everything collides by circle
 * overlap.
 *
 * What makes this different from previous games in the series:
 *   - FLOATING-POINT physics, not grid cells
 *   - CONTINUOUS input handling (not per-tick direction changes)
 *   - 2D vector math (velocity, acceleration, wrap-around)
 *   - CIRCLE collision, not cell equality
 *   - Frame-rate-independent movement (dt-scaled updates)
 *
 * Controls:
 *   a / d   rotate left / right
 *   w       thrust forward
 *   space   fire bullet
 *   q       quit
 *
 * Usage:
 *   ./asteroids
 *
 * Skills practiced:
 *   - struct-based entities (ship, bullet, rock)
 *   - 2D vector arithmetic with float
 *   - Wrapping coordinates (torus topology)
 *   - Circle overlap collision detection
 *   - Bullet lifetime and rock splitting
 *   - Frame-independent update with delta time
 *   - Raw terminal mode + non-blocking input (from Day 49)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WIDTH         60
#define HEIGHT        24
#define MAX_BULLETS   16
#define MAX_ROCKS     12
#define TICK_MS       33     /* ~30 fps */
#define PI2           (2.0 * M_PI)

/* ---------- entities ---------- */

typedef struct
{
    float x, y;
    float vx, vy;
    float angle;      /* radians; 0 = pointing right */
} ship_t;

typedef struct
{
    float x, y;
    float vx, vy;
    int   alive;
    int   ttl;        /* ticks remaining */
} bullet_t;

typedef struct
{
    float x, y;
    float vx, vy;
    float radius;
    int   alive;
} rock_t;

typedef struct
{
    ship_t   ship;
    bullet_t bullets[MAX_BULLETS];
    rock_t   rocks[MAX_ROCKS];
    int      score;
    int      lives;
    int      game_over;
    int      thrusting;
} game_state;

/* ---------- terminal handling (from Day 49) ---------- */

static struct termios orig_term;
static int raw_mode = 0;

static void restore_terminal(void)
{
    if (raw_mode)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
        raw_mode = 0;
    }
    printf("\033[?25h\033[%d;1H\n", HEIGHT + 3);
    fflush(stdout);
}

static void enter_raw_mode(void)
{
    if (!isatty(STDIN_FILENO))
    {
        fprintf(stderr, "Error: stdin is not a terminal.\n");
        exit(EXIT_FAILURE);
    }
    if (tcgetattr(STDIN_FILENO, &orig_term) == -1)
    {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }
    struct termios raw = orig_term;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cflag |= CS8;
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
    raw_mode = 1;
    printf("\033[?25l");
    fflush(stdout);
}

static void handle_signal(int sig)
{
    (void)sig;
    restore_terminal();
    _exit(EXIT_SUCCESS);
}

/* Drain all pending input (raw mode sends one char at a time) */
static int read_keys(char *out, int max)
{
    int n = 0;
    unsigned char c;
    while (n < max && read(STDIN_FILENO, &c, 1) == 1)
        out[n++] = (char)c;
    return n;
}

/* ---------- vector helpers ---------- */

static float wrap_x(float x) { while (x < 0) x += WIDTH;  while (x >= WIDTH)  x -= WIDTH;  return x; }
static float wrap_y(float y) { while (y < 0) y += HEIGHT; while (y >= HEIGHT) y -= HEIGHT; return y; }

static float randf(float lo, float hi)
{
    return lo + (float)rand() / (float)RAND_MAX * (hi - lo);
}

/* ---------- world initialization ---------- */

static void spawn_rock(game_state *g, float x, float y, float radius)
{
    for (int i = 0; i < MAX_ROCKS; i++)
    {
        if (!g->rocks[i].alive)
        {
            g->rocks[i] = (rock_t){
                .x = x, .y = y,
                .vx = randf(-0.3f, 0.3f),
                .vy = randf(-0.15f, 0.15f),
                .radius = radius,
                .alive = 1,
            };
            return;
        }
    }
}

static void spawn_initial_rocks(game_state *g)
{
    for (int i = 0; i < 4; i++)
    {
        float x = randf(0, WIDTH);
        float y = randf(0, HEIGHT);
        /* Don't spawn on top of the ship */
        if (fabsf(x - g->ship.x) < 8 && fabsf(y - g->ship.y) < 6)
            x = wrap_x(x + WIDTH / 2);
        spawn_rock(g, x, y, 2.5f);
    }
}

static void game_init(game_state *g)
{
    memset(g, 0, sizeof(*g));
    g->ship = (ship_t){ .x = WIDTH / 2.0f, .y = HEIGHT / 2.0f,
                        .vx = 0, .vy = 0, .angle = 0 };
    g->lives = 3;
    g->score = 0;
    spawn_initial_rocks(g);
}

/* ---------- spawning bullets ---------- */

static void fire_bullet(game_state *g)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!g->bullets[i].alive)
        {
            /* Bullet inherits the ship's momentum plus a forward kick */
            g->bullets[i] = (bullet_t){
                .x = g->ship.x,
                .y = g->ship.y,
                .vx = g->ship.vx + cosf(g->ship.angle) * 1.5f,
                .vy = g->ship.vy * 0.5f + sinf(g->ship.angle) * 0.75f,
                .alive = 1,
                .ttl = 60,
            };
            return;
        }
    }
}

/* ---------- collision ---------- */

static int circles_overlap(float x1, float y1, float r1,
                           float x2, float y2, float r2)
{
    float dx = x1 - x2;
    float dy = y1 - y2;
    float r = r1 + r2;
    return dx * dx + dy * dy < r * r;
}

/* ---------- game update ---------- */

static void update(game_state *g, float dt)
{
    if (g->game_over)
        return;

    /* --- ship control --- */
    /* Thrust accelerates in the ship's facing direction */
    if (g->thrusting)
    {
        g->ship.vx += cosf(g->ship.angle) * 12.0f * dt;
        g->ship.vy += sinf(g->ship.angle) *  6.0f * dt;
    }

    /* Mild drag so the ship doesn't accelerate forever */
    g->ship.vx *= 0.99f;
    g->ship.vy *= 0.99f;

    /* Integrate position */
    g->ship.x = wrap_x(g->ship.x + g->ship.vx * dt);
    g->ship.y = wrap_y(g->ship.y + g->ship.vy * dt);

    /* --- bullets --- */
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!g->bullets[i].alive)
            continue;

        g->bullets[i].x = wrap_x(g->bullets[i].x + g->bullets[i].vx * dt * 60);
        g->bullets[i].y = wrap_y(g->bullets[i].y + g->bullets[i].vy * dt * 30);

        if (--g->bullets[i].ttl <= 0)
            g->bullets[i].alive = 0;
    }

    /* --- rocks --- */
    for (int i = 0; i < MAX_ROCKS; i++)
    {
        if (!g->rocks[i].alive)
            continue;
        g->rocks[i].x = wrap_x(g->rocks[i].x + g->rocks[i].vx * dt * 60);
        g->rocks[i].y = wrap_y(g->rocks[i].y + g->rocks[i].vy * dt * 30);
    }

    /* --- bullet vs rock --- */
    for (int b = 0; b < MAX_BULLETS; b++)
    {
        if (!g->bullets[b].alive)
            continue;

        for (int r = 0; r < MAX_ROCKS; r++)
        {
            if (!g->rocks[r].alive)
                continue;

            if (circles_overlap(g->bullets[b].x, g->bullets[b].y, 0.4f,
                                g->rocks[r].x,  g->rocks[r].y,  g->rocks[r].radius))
            {
                /* Destroy bullet */
                g->bullets[b].alive = 0;

                /* Split or destroy rock */
                float old_x = g->rocks[r].x;
                float old_y = g->rocks[r].y;
                float old_r = g->rocks[r].radius;
                g->rocks[r].alive = 0;

                if (old_r > 1.2f)
                {
                    /* Split into two smaller rocks */
                    spawn_rock(g, wrap_x(old_x - old_r / 2), wrap_y(old_y), old_r / 1.7f);
                    spawn_rock(g, wrap_x(old_x + old_r / 2), wrap_y(old_y), old_r / 1.7f);
                    g->score += 5;
                }
                else
                {
                    g->score += 20;
                }
                break;
            }
        }
    }

    /* --- rock vs ship --- */
    for (int r = 0; r < MAX_ROCKS; r++)
    {
        if (!g->rocks[r].alive)
            continue;

        if (circles_overlap(g->ship.x, g->ship.y, 0.8f,
                            g->rocks[r].x, g->rocks[r].y, g->rocks[r].radius))
        {
            /* Lose a life, reset ship, clear rocks near ship */
            g->lives--;
            g->ship.x = WIDTH / 2.0f;
            g->ship.y = HEIGHT / 2.0f;
            g->ship.vx = 0;
            g->ship.vy = 0;
            g->ship.angle = 0;
            if (g->lives <= 0)
            {
                g->game_over = 1;
            }
            break;
        }
    }

    /* --- win condition --- */
    int rock_count = 0;
    for (int r = 0; r < MAX_ROCKS; r++)
        if (g->rocks[r].alive) rock_count++;

    if (rock_count == 0)
        spawn_initial_rocks(g);   /* next wave */
}

/* ---------- rendering ---------- */

static void render(const game_state *g)
{
    /* Screen buffer. Each cell holds one character. */
    char screen[HEIGHT][WIDTH];
    memset(screen, ' ', sizeof(screen));

    /* Draw rocks (large: '*', small: 'o') */
    for (int i = 0; i < MAX_ROCKS; i++)
    {
        if (!g->rocks[i].alive) continue;
        int cx = (int)g->rocks[i].x;
        int cy = (int)g->rocks[i].y;
        if (cx < 0 || cx >= WIDTH || cy < 0 || cy >= HEIGHT) continue;

        char ch = (g->rocks[i].radius > 1.2f) ? '*' : 'o';
        /* Draw a small blob around center */
        int r = (int)g->rocks[i].radius;
        for (int dy = -r; dy <= r; dy++)
        {
            for (int dx = -r; dx <= r; dx++)
            {
                if (dx*dx + dy*dy > r*r) continue;
                int px = cx + dx, py = cy + dy;
                if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT)
                    screen[py][px] = ch;
            }
        }
    }

    /* Draw bullets */
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!g->bullets[i].alive) continue;
        int bx = (int)g->bullets[i].x;
        int by = (int)g->bullets[i].y;
        if (bx >= 0 && bx < WIDTH && by >= 0 && by < HEIGHT)
            screen[by][bx] = '.';
    }

    /* Draw ship (at its position, with a direction indicator) */
    int sx = (int)g->ship.x;
    int sy = (int)g->ship.y;
    if (sx >= 0 && sx < WIDTH && sy >= 0 && sy < HEIGHT)
        screen[sy][sx] = (g->thrusting) ? 'A' : '^';

    /* Output the screen */
    printf("\033[H");

    /* Top border */
    printf("+");
    for (int x = 0; x < WIDTH; x++) printf("-");
    printf("+\n");

    for (int y = 0; y < HEIGHT; y++)
    {
        printf("|");
        for (int x = 0; x < WIDTH; x++)
            putchar(screen[y][x]);
        printf("|\n");
    }

    /* Bottom border */
    printf("+");
    for (int x = 0; x < WIDTH; x++) printf("-");
    printf("+\n");

    printf("Score: %d   Lives: %d   ", g->score, g->lives);
    if (g->game_over)
        printf("\nGame over. Press 'r' to restart or 'q' to quit.\n");
    else
        printf("\r");

    fflush(stdout);
}

/* ---------- main ---------- */

int main(void)
{
    srand((unsigned)time(NULL));

    atexit(restore_terminal);
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    enter_raw_mode();

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags != -1)
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    game_state g;
    game_init(&g);

    printf("\033[2J");

    struct timespec tick = { 0, TICK_MS * 1000000L };
    const float dt = TICK_MS / 1000.0f;

    while (1)
    {
        /* Drain all pending input this tick */
        char keys[32];
        int nkeys = read_keys(keys, sizeof(keys));

        for (int i = 0; i < nkeys; i++)
        {
            char k = keys[i];

            if (k == 'q' || k == 'Q')
            {
                restore_terminal();
                return 0;
            }

            if ((k == 'r' || k == 'R') && g.game_over)
            {
                game_init(&g);
                continue;
            }

            if (g.game_over)
                continue;

            switch (k)
            {
                case 'a': case 'A': case 'h':
                    g.ship.angle -= 0.35f;
                    break;
                case 'd': case 'D': case 'l':
                    g.ship.angle += 0.35f;
                    break;
                case 'w': case 'W': case 'k':
                    g.thrusting = 1;
                    break;
                case ' ': case 'f':
                    fire_bullet(&g);
                    break;
            }
        }

        update(&g, dt);

        /* Thrusting is a per-tick state: if no 'w' was pressed
         * this tick, it's off. This is a design choice; the
         * alternative is to hold it on until explicitly released,
         * but in a key-at-a-time terminal we can't detect release. */
        g.thrusting = 0;

        render(&g);

        nanosleep(&tick, NULL);
    }

    restore_terminal();
    return 0;
}

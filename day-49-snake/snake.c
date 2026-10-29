/* snake.c
 *
 * A terminal Snake game with real-time input.
 *
 * Controls:
 *   WASD / hjkl   move
 *   q             quit
 *
 * The game runs at a fixed tick rate. On each tick the snake
 * advances one cell, the board is redrawn, and input is polled
 * non-blocking. If the snake eats food, it grows by one. If it
 * hits a wall or itself, the game ends.
 *
 * What makes this different from previous games in the series:
 *   - REAL-TIME input: no "press Enter to move"
 *   - RAW terminal mode: characters arrive as typed
 *   - NON-BLOCKING read: the game loop never waits for input
 *   - Fixed-frame rendering: full redraw each tick
 *
 * The terminal is put into raw mode with termios; on exit the
 * original settings are restored, even if the game crashes.
 *
 * Usage:
 *   ./snake
 *
 * Skills practiced:
 *   - termios for raw terminal mode
 *   - Non-blocking read with fcntl(O_NONBLOCK)
 *   - ANSI escape codes for cursor positioning and clearing
 *   - Fixed-timestep game loop with nanosleep
 *   - Linked-list-style snake body on a 2D grid
 *   - 2D array board with wrap-around-free boundary checking
 *   - Signal handling to restore the terminal on Ctrl+C
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define WIDTH   40
#define HEIGHT  20
#define TICK_MS 100
#define MAX_SNAKE (WIDTH * HEIGHT)

/* Directions */
enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

/* Game state */
typedef struct
{
    int x, y;      /* head position */
} point;

typedef struct
{
    point body[MAX_SNAKE];   /* body[0] is the head */
    int length;
    int dir;
    int next_dir;            /* queued direction from input */
    point food;
    int score;
    int game_over;
    int won;
} game_state;

/* Terminal state, saved so we can restore on exit */
static struct termios original_term;
static int raw_mode_active = 0;

/* ---------- terminal control ---------- */

/* Restore the terminal to its original state. Called on exit
 * and on SIGINT/SIGTERM. */
static void restore_terminal(void)
{
    if (raw_mode_active)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_term);
        raw_mode_active = 0;
    }
    /* Show cursor, move to bottom of screen */
    printf("\033[?25h");
    printf("\033[%d;1H\n", HEIGHT + 3);
    fflush(stdout);
}

/* Put the terminal into raw mode: no line buffering, no echo */
static void enter_raw_mode(void)
{
    if (!isatty(STDIN_FILENO))
    {
        fprintf(stderr, "Error: stdin is not a terminal.\n");
        exit(EXIT_FAILURE);
    }

    if (tcgetattr(STDIN_FILENO, &original_term) == -1)
    {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    struct termios raw = original_term;
    /* Disable canonical mode (line buffering), echo, signals */
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    /* Disable input translation and flow control */
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    /* 8-bit chars, no output post-processing */
    raw.c_cflag |= CS8;
    raw.c_oflag &= ~(OPOST);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
    raw_mode_active = 1;

    /* Hide cursor */
    printf("\033[?25l");
    fflush(stdout);
}

/* Signal handler: restore terminal, then exit */
static void handle_signal(int sig)
{
    (void)sig;
    restore_terminal();
    _exit(EXIT_SUCCESS);
}

/* Non-blocking read: returns -1 if no input, or the character */
static int read_key(void)
{
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 1)
        return c;
    return -1;
}

/* ---------- game initialization ---------- */

static void spawn_food(game_state *g)
{
    /* Count empty cells so we can pick one uniformly at random */
    int empty_count = 0;
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
        {
            int occupied = 0;
            for (int i = 0; i < g->length; i++)
                if (g->body[i].x == x && g->body[i].y == y)
                {
                    occupied = 1;
                    break;
                }
            if (!occupied)
                empty_count++;
        }

    if (empty_count == 0)
    {
        g->won = 1;
        g->game_over = 1;
        return;
    }

    int pick = rand() % empty_count;
    int idx = 0;
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
        {
            int occupied = 0;
            for (int i = 0; i < g->length; i++)
                if (g->body[i].x == x && g->body[i].y == y)
                {
                    occupied = 1;
                    break;
                }
            if (!occupied)
            {
                if (idx == pick)
                {
                    g->food.x = x;
                    g->food.y = y;
                    return;
                }
                idx++;
            }
        }
}

static void game_init(game_state *g)
{
    memset(g, 0, sizeof(*g));

    g->length = 3;
    g->dir = DIR_RIGHT;
    g->next_dir = DIR_RIGHT;

    /* Start with the snake horizontal, head in the middle */
    int sx = WIDTH / 2;
    int sy = HEIGHT / 2;
    for (int i = 0; i < g->length; i++)
    {
        g->body[i].x = sx - i;
        g->body[i].y = sy;
    }

    spawn_food(g);
}

/* ---------- game update ---------- */

/* Cell occupied by the snake body at index >= 1? (excluding head) */
static int snake_contains(const game_state *g, int x, int y, int skip_head)
{
    int start = skip_head ? 1 : 0;
    for (int i = start; i < g->length; i++)
        if (g->body[i].x == x && g->body[i].y == y)
            return 1;
    return 0;
}

static void game_tick(game_state *g)
{
    if (g->game_over)
        return;

    /* Apply queued direction, but prevent reversing */
    int d = g->next_dir;
    if ((d == DIR_UP    && g->dir != DIR_DOWN) ||
        (d == DIR_DOWN  && g->dir != DIR_UP)   ||
        (d == DIR_LEFT  && g->dir != DIR_RIGHT)||
        (d == DIR_RIGHT && g->dir != DIR_LEFT))
        g->dir = d;

    /* Compute new head position */
    point new_head = g->body[0];
    switch (g->dir)
    {
        case DIR_UP:    new_head.y--; break;
        case DIR_DOWN:  new_head.y++; break;
        case DIR_LEFT:  new_head.x--; break;
        case DIR_RIGHT: new_head.x++; break;
    }

    /* Wall collision */
    if (new_head.x < 0 || new_head.x >= WIDTH ||
        new_head.y < 0 || new_head.y >= HEIGHT)
    {
        g->game_over = 1;
        return;
    }

    /* Self collision (the tail will move away unless we grow,
     * so skip checking the last segment) */
    int eating = (new_head.x == g->food.x && new_head.y == g->food.y);
    if (snake_contains(g, new_head.x, new_head.y, 0) &&
        !(!eating && new_head.x == g->body[g->length - 1].x &&
          new_head.y == g->body[g->length - 1].y))
    {
        g->game_over = 1;
        return;
    }

    /* Shift body: each segment moves to where the previous one was */
    for (int i = g->length - 1; i > 0; i--)
        g->body[i] = g->body[i - 1];

    g->body[0] = new_head;

    /* Grow if we ate; otherwise the tail shrank (we shift) */
    if (eating)
    {
        g->length++;
        g->score += 10;
        spawn_food(g);
    }
}

/* ---------- rendering ---------- */

static void render(const game_state *g)
{
    /* Move cursor to top-left without clearing (flicker-free) */
    printf("\033[H");

    /* Border top */
    printf("+");
    for (int x = 0; x < WIDTH; x++)
        printf("-");
    printf("+\n");

    for (int y = 0; y < HEIGHT; y++)
    {
        printf("|");
        for (int x = 0; x < WIDTH; x++)
        {
            char cell = ' ';

            if (x == g->body[0].x && y == g->body[0].y)
                cell = '@';
            else if (snake_contains(g, x, y, 1))
                cell = 'o';
            else if (x == g->food.x && y == g->food.y)
                cell = '*';

            putchar(cell);
        }
        printf("|\n");
    }

    /* Border bottom */
    printf("+");
    for (int x = 0; x < WIDTH; x++)
        printf("-");
    printf("+\n");

    printf("Score: %d   Length: %d   ", g->score, g->length);

    if (g->game_over)
    {
        if (g->won)
            printf("\nYou win! The board is full.\n");
        else
            printf("\nGame over. Press q to quit.\n");
    }
    else
    {
        printf("\r");
    }

    fflush(stdout);
}

/* ---------- main loop ---------- */

int main(void)
{
    srand((unsigned)time(NULL));

    /* Install cleanup handlers before touching the terminal */
    atexit(restore_terminal);
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    enter_raw_mode();

    /* Set stdin non-blocking so read_key never waits */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags != -1)
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    game_state g;
    game_init(&g);

    /* Clear screen and hide cursor once */
    printf("\033[2J");

    struct timespec tick = { 0, TICK_MS * 1000000L };

    while (1)
    {
        /* Poll input */
        int key = read_key();
        if (key == 'q' || key == 'Q')
            break;

        switch (key)
        {
            case 'w': case 'k': g.next_dir = DIR_UP;    break;
            case 's': case 'j': g.next_dir = DIR_DOWN;  break;
            case 'a': case 'h': g.next_dir = DIR_LEFT;  break;
            case 'd': case 'l': g.next_dir = DIR_RIGHT; break;
            case 'r':
                if (g.game_over)
                    game_init(&g);
                break;
            default: break;
        }

        game_tick(&g);
        render(&g);

        /* Sleep for one tick */
        nanosleep(&tick, NULL);
    }

    restore_terminal();
    return 0;
}

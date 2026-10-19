/* 2048.c
 *
 * A terminal version of the sliding-tile puzzle 2048.
 *
 * The board is a 4x4 grid. Each cell holds 0 (empty) or a power
 * of two. On each move, all tiles slide in one direction; equal
 * neighbours merge into their sum, and a new tile (2 or 4)
 * spawns in a random empty cell.
 *
 * The goal is to build a tile with value 2048. The game ends
 * when no move changes the board.
 *
 * Controls:
 *   w / a / s / d   slide up / left / down / right
 *   q               quit
 *
 * Usage:
 *   ./2048
 *
 * Skills practiced:
 *   - 2D arrays and in-place transformations
 *   - The "compress, merge, compress" algorithm for sliding
 *   - Randomness with rand() and seeding with srand()
 *   - Game loop with input handling
 *   - Detecting "no moves left" without copying the board
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define SIZE 4
#define TARGET 2048

typedef struct
{
    int cells[SIZE][SIZE];
    int score;
} board;

/* ---------- randomness ---------- */

static void seed_random(void)
{
    srand((unsigned)time(NULL));
}

/* Return a random int in [lo, hi] inclusive */
static int rand_int(int lo, int hi)
{
    return lo + rand() % (hi - lo + 1);
}

/* ---------- board operations ---------- */

static void board_clear(board *b)
{
    memset(b, 0, sizeof(*b));
}

/* Spawn a 2 (90%) or 4 (10%) in a random empty cell.
 * Returns 1 if a tile was placed, 0 if the board was full. */
static int board_spawn(board *b)
{
    int empty[SIZE * SIZE];
    int count = 0;

    for (int r = 0; r < SIZE; r++)
        for (int c = 0; c < SIZE; c++)
            if (b->cells[r][c] == 0)
                empty[count++] = r * SIZE + c;

    if (count == 0)
        return 0;

    int pick = empty[rand_int(0, count - 1)];
    b->cells[pick / SIZE][pick % SIZE] = (rand_int(1, 10) == 1) ? 4 : 2;
    return 1;
}

/* ---------- sliding algorithm ---------- */

/* One row (or column), left to right, compressed + merged + compressed.
 * This is the heart of 2048. */
static int slide_line(int *line, int *score)
{
    int out[SIZE] = {0};
    int outlen = 0;
    int moved = 0;

    /* Phase 1: compress - drop zeros */
    int tmp[SIZE];
    int tmplen = 0;
    for (int i = 0; i < SIZE; i++)
        if (line[i] != 0)
            tmp[tmplen++] = line[i];

    /* Phase 2: merge - only adjacent equals merge, and only once */
    int merged[SIZE];
    int mergedlen = 0;
    int i = 0;
    while (i < tmplen)
    {
        if (i + 1 < tmplen && tmp[i] == tmp[i + 1])
        {
            int sum = tmp[i] * 2;
            merged[mergedlen++] = sum;
            *score += sum;
            i += 2;
        }
        else
        {
            merged[mergedlen++] = tmp[i];
            i += 1;
        }
    }

    /* Phase 3: compress again - pad with zeros */
    for (int j = 0; j < mergedlen; j++)
        out[outlen++] = merged[j];

    for (int j = 0; j < SIZE; j++)
    {
        if (line[j] != out[j])
            moved = 1;
        line[j] = out[j];
    }

    return moved;
}

/* ---------- direction moves ---------- */

/* Extract row r, slide it, put it back */
static int move_row_left(board *b, int r)
{
    return slide_line(b->cells[r], &b->score);
}

/* Reverse the row, slide left, reverse back -> slide right */
static int move_row_right(board *b, int r)
{
    int rev[SIZE];
    for (int i = 0; i < SIZE; i++)
        rev[i] = b->cells[r][SIZE - 1 - i];

    int moved = slide_line(rev, &b->score);

    for (int i = 0; i < SIZE; i++)
        b->cells[r][SIZE - 1 - i] = rev[i];

    return moved;
}

static int move_up(board *b)
{
    int moved = 0;
    for (int c = 0; c < SIZE; c++)
    {
        int col[SIZE];
        for (int r = 0; r < SIZE; r++)
            col[r] = b->cells[r][c];

        moved |= slide_line(col, &b->score);

        for (int r = 0; r < SIZE; r++)
            b->cells[r][c] = col[r];
    }
    return moved;
}

static int move_down(board *b)
{
    int moved = 0;
    for (int c = 0; c < SIZE; c++)
    {
        int col[SIZE];
        for (int r = 0; r < SIZE; r++)
            col[r] = b->cells[SIZE - 1 - r][c];

        moved |= slide_line(col, &b->score);

        for (int r = 0; r < SIZE; r++)
            b->cells[SIZE - 1 - r][c] = col[r];
    }
    return moved;
}

static int move_left(board *b)
{
    int moved = 0;
    for (int r = 0; r < SIZE; r++)
        moved |= move_row_left(b, r);
    return moved;
}

static int move_right(board *b)
{
    int moved = 0;
    for (int r = 0; r < SIZE; r++)
        moved |= move_row_right(b, r);
    return moved;
}

/* ---------- state checks ---------- */

static int board_has_target(const board *b)
{
    for (int r = 0; r < SIZE; r++)
        for (int c = 0; c < SIZE; c++)
            if (b->cells[r][c] >= TARGET)
                return 1;
    return 0;
}

/* No moves left if board is full AND no two neighbours match */
static int board_stuck(const board *b)
{
    for (int r = 0; r < SIZE; r++)
        for (int c = 0; c < SIZE; c++)
            if (b->cells[r][c] == 0)
                return 0;

    for (int r = 0; r < SIZE; r++)
    {
        for (int c = 0; c < SIZE; c++)
        {
            if (c + 1 < SIZE && b->cells[r][c] == b->cells[r][c + 1])
                return 0;
            if (r + 1 < SIZE && b->cells[r][c] == b->cells[r + 1][c])
                return 0;
        }
    }
    return 1;
}

/* ---------- rendering ---------- */

static void print_board(const board *b)
{
    printf("Score: %d\n", b->score);
    printf("+------+------+------+------+\n");
    for (int r = 0; r < SIZE; r++)
    {
        printf("|");
        for (int c = 0; c < SIZE; c++)
        {
            if (b->cells[r][c] == 0)
                printf("      |");
            else
                printf(" %4d |", b->cells[r][c]);
        }
        printf("\n");
        printf("+------+------+------+------+\n");
    }
}

/* ---------- input ---------- */

static char read_command(void)
{
    char buf[16];
    if (fgets(buf, sizeof(buf), stdin) == NULL)
        return 'q';
    return (char)tolower((unsigned char)buf[0]);
}

/* ---------- main loop ---------- */

int main(void)
{
    board b;
    board_clear(&b);
    seed_random();

    /* Start with two tiles, as in the original game */
    board_spawn(&b);
    board_spawn(&b);

    printf("====================================\n");
    printf("              2048                  \n");
    printf("====================================\n");
    printf("w/a/s/d to slide, q to quit\n\n");

    for (;;)
    {
        print_board(&b);

        if (board_has_target(&b))
        {
            printf("\nYou reached %d! Well played.\n", TARGET);
            break;
        }

        if (board_stuck(&b))
        {
            printf("\nNo moves left. Final score: %d\n", b.score);
            break;
        }

        printf("\nMove (w/a/s/d, q to quit): ");
        fflush(stdout);

        char cmd = read_command();
        if (cmd == 'q')
        {
            printf("Bye.\n");
            break;
        }

        int moved = 0;
        switch (cmd)
        {
            case 'w': moved = move_up(&b);    break;
            case 'a': moved = move_left(&b);  break;
            case 's': moved = move_down(&b);  break;
            case 'd': moved = move_right(&b); break;
            default:  printf("Unknown command.\n");
        }

        if (moved)
            board_spawn(&b);

        printf("\n");
    }

    return 0;
}

/* minesweeper.c
 *
 * A simplified text-based Minesweeper in C.
 *
 * The board is an N×N grid with M mines.
 * The player reveals cells one at a time.
 * If a cell has no adjacent mines, its neighbors are revealed too.
 * Hitting a mine ends the game.
 *
 * Usage:
 *   ./minesweeper <size> <mines>
 *
 * Example:
 *   ./minesweeper 8 10
 *
 * Skills practiced:
 *   - 2D arrays
 *   - Recursion (flood fill)
 *   - Randomness (mine placement)
 *   - Game state
 *   - Input validation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>

#define MAX_SIZE 20

/* Cell states */
typedef struct {
    bool mine;       /* is there a mine? */
    bool revealed;   /* has the player revealed it? */
    bool flagged;    /* has the player flagged it? */
    int  adjacent;   /* number of adjacent mines */
} Cell;

/* The board */
typedef struct {
    Cell cells[MAX_SIZE][MAX_SIZE];
    int size;
    int mines;
    int revealed_count;
    bool game_over;
    bool won;
} Board;

/* --- Initialization --- */

void board_init(Board *b, int size, int mines)
{
    b->size = size;
    b->mines = mines;
    b->revealed_count = 0;
    b->game_over = false;
    b->won = false;

    for (int r = 0; r < size; r++)
        for (int c = 0; c < size; c++)
        {
            b->cells[r][c].mine = false;
            b->cells[r][c].revealed = false;
            b->cells[r][c].flagged = false;
            b->cells[r][c].adjacent = 0;
        }

    /* Place mines randomly */
    int placed = 0;
    while (placed < mines)
    {
        int r = rand() % size;
        int c = rand() % size;
        if (!b->cells[r][c].mine)
        {
            b->cells[r][c].mine = true;
            placed++;
        }
    }

    /* Compute adjacent counts */
    for (int r = 0; r < size; r++)
    {
        for (int c = 0; c < size; c++)
        {
            int count = 0;
            for (int dr = -1; dr <= 1; dr++)
                for (int dc = -1; dc <= 1; dc++)
                {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr;
                    int nc = c + dc;
                    if (nr >= 0 && nr < size && nc >= 0 && nc < size &&
                        b->cells[nr][nc].mine)
                        count++;
                }
            b->cells[r][c].adjacent = count;
        }
    }
}

/* --- Reveal (flood fill) --- */

void board_reveal(Board *b, int r, int c)
{
    if (r < 0 || r >= b->size || c < 0 || c >= b->size)
        return;
    if (b->cells[r][c].revealed || b->cells[r][c].flagged)
        return;

    b->cells[r][c].revealed = true;
    b->revealed_count++;

    if (b->cells[r][c].mine)
    {
        b->game_over = true;
        return;
    }

    if (b->cells[r][c].adjacent == 0)
    {
        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++)
            {
                if (dr == 0 && dc == 0) continue;
                board_reveal(b, r + dr, c + dc);
            }
    }
}

/* --- Print --- */

void board_print(const Board *b)
{
    printf("\n    ");
    for (int c = 0; c < b->size; c++)
        printf("%2d ", c);
    printf("\n");

    for (int r = 0; r < b->size; r++)
    {
        printf("%2d  ", r);
        for (int c = 0; c < b->size; c++)
        {
            const Cell *cell = &b->cells[r][c];

            if (b->game_over && cell->mine)
                printf(" * ");
            else if (cell->flagged)
                printf(" F ");
            else if (!cell->revealed)
                printf(" . ");
            else if (cell->adjacent == 0)
                printf("   ");
            else
                printf(" %d ", cell->adjacent);
        }
        printf("\n");
    }
    printf("\n");
}

/* --- Game logic --- */

bool board_check_win(const Board *b)
{
    return b->revealed_count == b->size * b->size - b->mines;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <size> <mines>\n", argv[0]);
        return 1;
    }

    int size = atoi(argv[1]);
    int mines = atoi(argv[2]);

    if (size < 2 || size > MAX_SIZE)
    {
        fprintf(stderr, "Error: size must be between 2 and %d.\n", MAX_SIZE);
        return 1;
    }
    if (mines < 1 || mines >= size * size)
    {
        fprintf(stderr, "Error: mines must be between 1 and %d.\n", size * size - 1);
        return 1;
    }

    srand((unsigned) time(NULL));

    Board b;
    board_init(&b, size, mines);

    printf("====================================\n");
    printf("           MINESWEEPER              \n");
    printf("====================================\n\n");
    printf("Board: %d×%d, Mines: %d\n", size, size, mines);
    printf("Enter: row col  (reveal)\n");
    printf("       f row col (flag)\n");
    printf("       q        (quit)\n");

    while (!b.game_over)
    {
        board_print(&b);

        printf("Your move: ");
        char cmd;
        scanf(" %c", &cmd);

        if (cmd == 'q' || cmd == 'Q')
        {
            printf("Quitting.\n");
            return 0;
        }

        int r, c;

        if (cmd == 'f' || cmd == 'F')
        {
            if (scanf("%d %d", &r, &c) != 2) continue;
            if (r < 0 || r >= size || c < 0 || c >= size) continue;
            if (b.cells[r][c].revealed) continue;

            b.cells[r][c].flagged = !b.cells[r][c].flagged;
            continue;
        }

        /* Treat the first character as a digit (row) */
        if (!isdigit((unsigned char) cmd)) continue;

        r = cmd - '0';
        if (scanf("%d", &c) != 1) continue;

        if (r < 0 || r >= size || c < 0 || c >= size) continue;
        if (b.cells[r][c].flagged) continue;

        board_reveal(&b, r, c);

        if (b.game_over)
        {
            board_print(&b);
            printf("💥 BOOM! You hit a mine. Game over.\n");
            return 0;
        }

        if (board_check_win(&b))
        {
            b.won = true;
            board_print(&b);
            printf("🎉 You cleared the board! You win!\n");
            return 0;
        }
    }

    return 0;
}

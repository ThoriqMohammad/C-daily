/* connect_four.c
 *
 * A two-player Connect Four game with an optional minimax AI.
 *
 * The board is 7 columns wide and 6 rows tall. Players drop
 * discs into columns; gravity puts each disc on the lowest
 * empty row. The first player to line up four of their own
 * discs—horizontally, vertically, or diagonally—wins.
 *
 * Modes:
 *   1  human vs human
 *   2  human vs AI (human moves first)
 *   3  AI vs AI (useful for testing the AI)
 *
 * The AI uses minimax with alpha-beta pruning. It does not do
 * full-board search (that would need a deeper horizon); it
 * evaluates 5 plies ahead, which is enough to play a decent
 * game without an opening book.
 *
 * Usage:
 *   ./connect_four
 *
 * Skills practiced:
 *   - 2D array game state with gravity
 *   - Direction-based win detection (four axes)
 *   - Recursive minimax with alpha-beta pruning
 *   - Heuristic evaluation (patterns of three vs two)
 *   - Menu loop and input handling
 *   - Alternating turns with early termination
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ROWS      6
#define COLS      7
#define WIN_LEN   4
#define SEARCH_DEPTH 5

#define EMPTY 0
#define HUMAN 1
#define AI    2

typedef struct
{
    int cells[ROWS][COLS];   /* row 0 is the top */
    int turn;                /* HUMAN or AI */
    int winner;              /* EMPTY, HUMAN, AI, or -1 for draw */
} game;

/* ---------- board setup ---------- */

void game_init(game *g, int first_turn)
{
    memset(g->cells, 0, sizeof(g->cells));
    g->turn = first_turn;
    g->winner = EMPTY;
}

/* Lowest empty row in a column, or -1 if full */
int lowest_empty_row(const game *g, int col)
{
    for (int r = ROWS - 1; r >= 0; r--)
        if (g->cells[r][col] == EMPTY)
            return r;
    return -1;
}

/* Column is full? */
int column_full(const game *g, int col)
{
    return lowest_empty_row(g, col) == -1;
}

/* Board is full? */
int board_full(const game *g)
{
    for (int c = 0; c < COLS; c++)
        if (!column_full(g, c))
            return 0;
    return 1;
}

/* ---------- win detection ---------- */

/* Count consecutive 'player' discs starting at (r,c) going in
 * direction (dr,dc). Returns the length of the run that includes
 * (r,c) plus its continuation in that direction. */
static int count_dir(const game *g, int r, int c, int dr, int dc, int player)
{
    int n = 0;
    while (r >= 0 && r < ROWS && c >= 0 && c < COLS && g->cells[r][c] == player)
    {
        n++;
        r += dr;
        c += dc;
    }
    return n;
}

/* Does player have four in a row anywhere? */
int player_has_won(const game *g, int player)
{
    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (g->cells[r][c] != player)
                continue;

            /* Four directions to check: horizontal, vertical,
             * diagonal-down-right, diagonal-down-left */
            if (count_dir(g, r, c, 0, 1, player) >= WIN_LEN)
                return 1;
            if (count_dir(g, r, c, 1, 0, player) >= WIN_LEN)
                return 1;
            if (count_dir(g, r, c, 1, 1, player) >= WIN_LEN)
                return 1;
            if (count_dir(g, r, c, 1, -1, player) >= WIN_LEN)
                return 1;
        }
    }
    return 0;
}

/* ---------- moves ---------- */

/* Drop a disc for 'player' into 'col'. Returns 1 on success,
 * 0 if the column was full. */
int make_move(game *g, int col, int player)
{
    int r = lowest_empty_row(g, col);
    if (r == -1)
        return 0;

    g->cells[r][col] = player;
    return 1;
}

/* Undo the topmost disc in 'col' */
void undo_move(game *g, int col)
{
    for (int r = 0; r < ROWS; r++)
    {
        if (g->cells[r][col] != EMPTY)
        {
            g->cells[r][col] = EMPTY;
            return;
        }
    }
}

/* ---------- heuristic ---------- */

/* A simple positional score: reward patterns of 3 and 2 of the
 * player's own discs, penalize the opponent's. Center columns
 * are worth a bit more because they participate in more winning
 * lines. */
int evaluate(const game *g, int player)
{
    int opp = (player == HUMAN) ? AI : HUMAN;
    int score = 0;

    /* Center column bonus */
    for (int r = 0; r < ROWS; r++)
    {
        if (g->cells[r][COLS / 2] == player) score += 3;
        if (g->cells[r][COLS / 2] == opp)    score -= 3;
    }

    /* Scan every 4-cell window in four directions */
    static const int dirs[4][2] = { {0,1}, {1,0}, {1,1}, {1,-1} };

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            for (int d = 0; d < 4; d++)
            {
                int dr = dirs[d][0];
                int dc = dirs[d][1];

                int rr = r + (WIN_LEN - 1) * dr;
                int cc = c + (WIN_LEN - 1) * dc;
                if (rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS)
                    continue;

                int mine = 0, theirs = 0;
                for (int i = 0; i < WIN_LEN; i++)
                {
                    int v = g->cells[r + i * dr][c + i * dc];
                    if (v == player) mine++;
                    else if (v == opp) theirs++;
                }

                if (mine > 0 && theirs == 0)
                {
                    if (mine == 3) score += 50;
                    else if (mine == 2) score += 10;
                    else if (mine == 1) score += 1;
                }
                else if (theirs > 0 && mine == 0)
                {
                    if (theirs == 3) score -= 50;
                    else if (theirs == 2) score -= 10;
                    else if (theirs == 1) score -= 1;
                }
            }
        }
    }

    return score;
}

/* ---------- minimax with alpha-beta ---------- */

static int minimax(game *g, int depth, int alpha, int beta,
                   int maximizing, int ai_player)
{
    /* Terminal: someone won, or the board is full */
    if (player_has_won(g, ai_player))
        return 100000 + depth;   /* prefer quicker wins */
    if (player_has_won(g, (ai_player == HUMAN) ? AI : HUMAN))
        return -100000 - depth;
    if (board_full(g) || depth == 0)
        return evaluate(g, ai_player);

    if (maximizing)
    {
        int best = -1000000;
        for (int c = 0; c < COLS; c++)
        {
            if (column_full(g, c))
                continue;
            make_move(g, c, ai_player);
            int score = minimax(g, depth - 1, alpha, beta, 0, ai_player);
            undo_move(g, c);
            if (score > best) best = score;
            if (best > alpha) alpha = best;
            if (beta <= alpha) break;
        }
        return best;
    }
    else
    {
        int opp = (ai_player == HUMAN) ? AI : HUMAN;
        int best = 1000000;
        for (int c = 0; c < COLS; c++)
        {
            if (column_full(g, c))
                continue;
            make_move(g, c, opp);
            int score = minimax(g, depth - 1, alpha, beta, 1, ai_player);
            undo_move(g, c);
            if (score < best) best = score;
            if (best < beta) beta = best;
            if (beta <= alpha) break;
        }
        return best;
    }
}

/* Pick the best column for the AI to play, or -1 if no move */
int ai_pick_move(game *g, int ai_player)
{
    int best_col = -1;
    int best_score = -1000000;

    for (int c = 0; c < COLS; c++)
    {
        if (column_full(g, c))
            continue;

        make_move(g, c, ai_player);
        int score = minimax(g, SEARCH_DEPTH - 1, -1000000, 1000000, 0, ai_player);
        undo_move(g, c);

        if (score > best_score)
        {
            best_score = score;
            best_col = c;
        }
    }

    return best_col;
}

/* ---------- rendering ---------- */

void print_board(const game *g)
{
    printf("\n  ");
    for (int c = 0; c < COLS; c++)
        printf(" %d  ", c + 1);
    printf("\n");

    printf(" +");
    for (int c = 0; c < COLS; c++)
        printf("---+");
    printf("\n");

    for (int r = 0; r < ROWS; r++)
    {
        printf(" |");
        for (int c = 0; c < COLS; c++)
        {
            char ch = ' ';
            if (g->cells[r][c] == HUMAN) ch = 'X';
            else if (g->cells[r][c] == AI) ch = 'O';
            printf(" %c |", ch);
        }
        printf("\n");
        printf(" +");
        for (int c = 0; c < COLS; c++)
            printf("---+");
        printf("\n");
    }
}

/* ---------- input ---------- */

static int read_column(void)
{
    char buf[16];
    if (fgets(buf, sizeof(buf), stdin) == NULL)
        return -1;
    if (buf[0] == 'q' || buf[0] == 'Q')
        return -1;
    int col = atoi(buf);
    return col - 1;   /* convert 1-based input to 0-based index */
}

/* ---------- game loop ---------- */

static int read_mode(void)
{
    printf("Choose mode:\n");
    printf("  1  human vs human\n");
    printf("  2  human (X) vs AI (O)\n");
    printf("  3  AI vs AI (demo)\n");
    printf("Choice: ");
    fflush(stdout);

    char buf[16];
    if (fgets(buf, sizeof(buf), stdin) == NULL)
        return 2;
    return atoi(buf);
}

int main(void)
{
    game g;
    game_init(&g, HUMAN);

    printf("====================================\n");
    printf("          CONNECT FOUR              \n");
    printf("====================================\n");

    int mode = read_mode();

    if (mode != 1 && mode != 2 && mode != 3)
        mode = 2;

    int p1 = HUMAN, p2 = (mode == 1) ? HUMAN : AI;

    for (;;)
    {
        print_board(&g);

        int mover = g.turn;
        int is_ai = (mode == 3) || (mode == 2 && mover == AI);

        int col = -1;
        if (is_ai)
        {
            col = ai_pick_move(&g, mover);
            printf("AI plays column %d\n", col + 1);
        }
        else
        {
            printf("Player %c, choose column (1-%d, q to quit): ",
                   mover == HUMAN ? 'X' : 'O', COLS);
            fflush(stdout);
            col = read_column();
            if (col == -1)
            {
                printf("Bye.\n");
                return 0;
            }
            if (col < 0 || col >= COLS || column_full(&g, col))
            {
                printf("Invalid column, try again.\n");
                continue;
            }
        }

        if (!make_move(&g, col, mover))
        {
            printf("Column is full, try again.\n");
            continue;
        }

        if (player_has_won(&g, mover))
        {
            print_board(&g);
            printf("\nPlayer %c wins!\n", mover == HUMAN ? 'X' : 'O');
            break;
        }

        if (board_full(&g))
        {
            print_board(&g);
            printf("\nDraw — the board is full.\n");
            break;
        }

        g.turn = (mover == p1) ? p2 : p1;
    }

    return 0;
}

/* minesweeper_solver.c
 *
 * A Minesweeper solver using constraint propagation and
 * subset inference.
 *
 * Given a Minesweeper board where all cells are revealed
 * except some, the solver determines which unrevealed cells
 * are safe to click and which are mines. It never guesses
 * randomly — it only reveals cells it has proven safe.
 *
 * The two techniques:
 *
 *   1. DIRECT COUNTING
 *      If a revealed number N has exactly N hidden neighbors,
 *      all those neighbors are mines.
 *      If a revealed number N has exactly N flagged neighbors,
 *      all other neighbors are safe.
 *
 *   2. SUBSET INFERENCE
 *      If constraint A = {cells, count} and constraint B =
 *      {cells, count} have A.cells ⊆ B.cells and
 *      A.count == B.count, then B.cells \ A.cells are all safe.
 *      Similarly, if A.cells ⊆ B.cells and B.count - A.count
 *      == |B.cells| - |A.cells|, then B.cells \ A.cells are
 *      all mines.
 *
 * These two rules solve the vast majority of expert-level
 * boards. What they can't solve — situations where every
 * remaining cell is ambiguous — is where humans also have to
 * guess.
 *
 * The program:
 *   - Takes a board description from stdin or a file
 *   - Applies both rules until no progress
 *   - Prints the cells proven safe, the cells proven mines,
 *     and the cells still ambiguous
 *
 * Board format (text):
 *   W H
 *   grid rows, each W chars:
 *     '?' = unrevealed (hidden)
 *     '.' = revealed empty (0 neighbors)
 *     '1'..'8' = revealed with that many mine neighbors
 *     '*' = known mine (flag placed)
 *
 * Usage:
 *   ./minesweeper_solver < board.txt
 *   ./minesweeper_solver --example     (built-in example)
 *
 * Skills practiced:
 *   - Constraint satisfaction
 *   - Set operations on cell coordinates (subset, difference)
 *   - Deductive inference (if X then Y)
 *   - Iteration until fixed point
 *   - Parsing a compact text format
 *   - Distinguishing "proven" from "guessed"
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_W 64
#define MAX_H 64
#define MAX_NEIGHBORS 8

/* Cell states */
enum
{
    CELL_HIDDEN = 0,
    CELL_REVEALED,   /* with count 0..8 in .count */
    CELL_MINE        /* flagged as a known mine */
};

typedef struct
{
    int state;
    int count;       /* number of adjacent mines (for revealed) */
} cell_t;

typedef struct
{
    int w, h;
    cell_t grid[MAX_H][MAX_W];
} board_t;

/* ---------- parsing ---------- */

static int parse_board(board_t *b, FILE *fp)
{
    if (fscanf(fp, "%d %d", &b->w, &b->h) != 2) return -1;
    if (b->w < 1 || b->w > MAX_W) return -1;
    if (b->h < 1 || b->h > MAX_H) return -1;

    /* Skip newline after dimensions */
    int c;
    while ((c = fgetc(fp)) != '\n' && c != EOF) { }

    for (int y = 0; y < b->h; y++)
    {
        char line[MAX_W + 4];
        if (fgets(line, sizeof(line), fp) == NULL) return -1;

        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (len < b->w) return -1;

        for (int x = 0; x < b->w; x++)
        {
            char ch = line[x];
            if (ch == '?')      { b->grid[y][x].state = CELL_HIDDEN; b->grid[y][x].count = 0; }
            else if (ch == '*') { b->grid[y][x].state = CELL_MINE;   b->grid[y][x].count = 0; }
            else if (ch == '.') { b->grid[y][x].state = CELL_REVEALED; b->grid[y][x].count = 0; }
            else if (ch >= '1' && ch <= '8')
            {
                b->grid[y][x].state = CELL_REVEALED;
                b->grid[y][x].count = ch - '0';
            }
            else return -1;
        }
    }
    return 0;
}

static void print_board(const board_t *b)
{
    for (int y = 0; y < b->h; y++)
    {
        for (int x = 0; x < b->w; x++)
        {
            const cell_t *c = &b->grid[y][x];
            if (c->state == CELL_HIDDEN)        putchar('?');
            else if (c->state == CELL_MINE)     putchar('*');
            else if (c->count == 0)             putchar('.');
            else                                putchar('0' + c->count);
        }
        putchar('\n');
    }
}

/* ---------- neighbor iteration ---------- */

/* Return the number of hidden or mine neighbors of (x,y) into
 * the caller-provided arrays. Each neighbor is given as (nx,ny).
 * Returns the count of neighbors filled. */
static int get_neighbors(int w, int h, int x, int y,
                         int nx[8], int ny[8])
{
    int n = 0;
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            if (dx == 0 && dy == 0) continue;
            int xx = x + dx, yy = y + dy;
            if (xx < 0 || xx >= w || yy < 0 || yy >= h) continue;
            nx[n] = xx;
            ny[n] = yy;
            n++;
        }
    }
    return n;
}

/* Count how many of the given neighbors are in a particular state */
static int count_state(const board_t *b, int n,
                       const int nx[8], const int ny[8], int state)
{
    int cnt = 0;
    for (int i = 0; i < n; i++)
        if (b->grid[ny[i]][nx[i]].state == state) cnt++;
    return cnt;
}

/* ---------- direct counting rule ---------- */

/* Apply the two direct-counting rules to every revealed cell.
 * Returns 1 if any cell was resolved (flag placed or cell
 * revealed), 0 if no progress. */
static int apply_direct_counting(board_t *b)
{
    int progress = 0;

    for (int y = 0; y < b->h; y++)
    {
        for (int x = 0; x < b->w; x++)
        {
            if (b->grid[y][x].state != CELL_REVEALED) continue;
            if (b->grid[y][x].count == 0) continue;

            int nx[8], ny[8];
            int n = get_neighbors(b->w, b->h, x, y, nx, ny);

            int hidden = count_state(b, n, nx, ny, CELL_HIDDEN);
            int mines  = count_state(b, n, nx, ny, CELL_MINE);

            /* Rule 1: if count == number of hidden + flagged mines,
             * then all hidden neighbors must be mines. */
            if (hidden > 0 && b->grid[y][x].count == hidden + mines)
            {
                for (int i = 0; i < n; i++)
                {
                    if (b->grid[ny[i]][nx[i]].state == CELL_HIDDEN)
                    {
                        b->grid[ny[i]][nx[i]].state = CELL_MINE;
                        progress = 1;
                    }
                }
            }

            /* Rule 2: if count == number of flagged mines,
             * then all other neighbors are safe. */
            if (hidden > 0 && b->grid[y][x].count == mines)
            {
                for (int i = 0; i < n; i++)
                {
                    if (b->grid[ny[i]][nx[i]].state == CELL_HIDDEN)
                    {
                        /* "Safe" for the solver means "revealed with
                         * whatever count it would have." We don't
                         * know the count, but we know it's not a mine.
                         * Set it as revealed with count 0, and rely on
                         * the caller to "click" it. For a pure logical
                         * solver we mark it revealed; the actual count
                         * would come from the real board. */
                        b->grid[ny[i]][nx[i]].state = CELL_REVEALED;
                        b->grid[ny[i]][nx[i]].count = 0;
                        progress = 1;
                    }
                }
            }
        }
    }

    return progress;
}

/* ---------- subset inference ---------- */

/* A constraint is: among the hidden cells in `cells`, exactly
 * `count` are mines. Derived from a revealed cell. */
typedef struct
{
    int cells[MAX_W * MAX_H];
    int ncells;
    int count;
} constraint_t;

/* Collect constraints from every revealed cell */
static int collect_constraints(const board_t *b,
                               constraint_t *out, int max_out)
{
    int n = 0;
    for (int y = 0; y < b->h; y++)
    {
        for (int x = 0; x < b->w; x++)
        {
            if (b->grid[y][x].state != CELL_REVEALED) continue;
            if (b->grid[y][x].count == 0) continue;

            int nx[8], ny[8];
            int nc = get_neighbors(b->w, b->h, x, y, nx, ny);

            int hidden_count = 0;
            int flagged = 0;
            int hidden_cells[MAX_NEIGHBORS];
            for (int i = 0; i < nc; i++)
            {
                int st = b->grid[ny[i]][nx[i]].state;
                if (st == CELL_HIDDEN)
                    hidden_cells[hidden_count++] = ny[i] * b->w + nx[i];
                else if (st == CELL_MINE)
                    flagged++;
            }

            if (hidden_count == 0) continue;
            if (n >= max_out) return n;

            constraint_t *c = &out[n++];
            c->ncells = hidden_count;
            for (int i = 0; i < hidden_count; i++)
                c->cells[i] = hidden_cells[i];
            c->count = b->grid[y][x].count - flagged;
        }
    }
    return n;
}

/* Is `cell` in the constraint's cells list? */
static int constraint_contains(const constraint_t *c, int cell)
{
    for (int i = 0; i < c->ncells; i++)
        if (c->cells[i] == cell) return 1;
    return 0;
}

/* Is A's cell set a subset of B's? */
static int is_subset(const constraint_t *a, const constraint_t *b)
{
    for (int i = 0; i < a->ncells; i++)
        if (!constraint_contains(b, a->cells[i])) return 0;
    return 1;
}

/* Apply subset inference: for pairs of constraints A ⊆ B, if
 * the "excess" cells have a determined mine/safe classification
 * based on the counts, mark them. */
static int apply_subset_inference(board_t *b)
{
    constraint_t cons[MAX_W * MAX_H];
    int ncons = collect_constraints(b, cons, MAX_W * MAX_H);
    int progress = 0;

    for (int i = 0; i < ncons; i++)
    {
        for (int j = 0; j < ncons; j++)
        {
            if (i == j) continue;
            if (!is_subset(&cons[i], &cons[j])) continue;

            /* B \ A */
            int diff[MAX_NEIGHBORS];
            int ndiff = 0;
            for (int k = 0; k < cons[j].ncells; k++)
                if (!constraint_contains(&cons[i], cons[j].cells[k]))
                    diff[ndiff++] = cons[j].cells[k];

            if (ndiff == 0) continue;

            int diff_count = cons[j].count - cons[i].count;

            /* Rule: if all diff cells are mines */
            if (diff_count == ndiff)
            {
                for (int k = 0; k < ndiff; k++)
                {
                    int cell = diff[k];
                    int y = cell / b->w, x = cell % b->w;
                    if (b->grid[y][x].state == CELL_HIDDEN)
                    {
                        b->grid[y][x].state = CELL_MINE;
                        progress = 1;
                    }
                }
            }
            /* Rule: if no diff cells are mines */
            else if (diff_count == 0)
            {
                for (int k = 0; k < ndiff; k++)
                {
                    int cell = diff[k];
                    int y = cell / b->w, x = cell % b->w;
                    if (b->grid[y][x].state == CELL_HIDDEN)
                    {
                        b->grid[y][x].state = CELL_REVEALED;
                        b->grid[y][x].count = 0;
                        progress = 1;
                    }
                }
            }
        }
    }

    return progress;
}

/* ---------- solve loop ---------- */

/* Apply both rules until no progress. Returns the number of
 * cells resolved (mines found + cells revealed). */
static int solve(board_t *b)
{
    int total_progress = 0;

    for (;;)
    {
        int p1 = apply_direct_counting(b);
        int p2 = apply_subset_inference(b);

        if (!p1 && !p2) break;
        total_progress = 1;
    }

    return total_progress;
}

/* ---------- analysis ---------- */

typedef struct
{
    int safe;
    int mines;
    int ambiguous;
} solve_stats_t;

static solve_stats_t analyze(const board_t *b)
{
    solve_stats_t s = {0, 0, 0};
    for (int y = 0; y < b->h; y++)
        for (int x = 0; x < b->w; x++)
        {
            int st = b->grid[y][x].state;
            if (st == CELL_HIDDEN) s.ambiguous++;
            else if (st == CELL_MINE) s.mines++;
        }
    /* "safe revealed" are cells that were hidden and are now
     * revealed. We count revealed cells with count 0 that were
     * initially hidden — but the board doesn't track initial
     * state. The solver's job is to prove cells safe; counting
     * ambiguous cells tells us how much remains. */
    return s;
}

/* ---------- example board ---------- */

/* A simple 4x4 with a known solution, given as text.
 * ? = hidden, . = revealed empty, 1-8 = revealed count. */
static const char *EXAMPLE_BOARD =
    "5 5\n"
    "..1??\n"
    ".12??\n"
    "1????\n"
    "?????\n"
    "?????\n";

/* ---------- main ---------- */

static void print_result(const board_t *b, solve_stats_t stats)
{
    printf("Solved board:\n");
    print_board(b);
    printf("\n");
    printf("Mines proven: %d\n", stats.mines);
    printf("Cells still ambiguous: %d\n", stats.ambiguous);

    if (stats.ambiguous == 0 && stats.mines > 0)
        printf("Fully solved without guessing.\n");
    else if (stats.ambiguous == 0)
        printf("No hidden cells remain.\n");
    else
        printf("Solver stalled on %d cell(s) — would need to guess.\n",
               stats.ambiguous);
}

int main(int argc, char *argv[])
{
    board_t b;

    if (argc >= 2 && strcmp(argv[1], "--example") == 0)
    {
        printf("====================================\n");
        printf("       MINESWEEPER SOLVER           \n");
        printf("====================================\n\n");

        printf("Input:\n%s\n", EXAMPLE_BOARD);

        FILE *fp = fmemopen((void *)EXAMPLE_BOARD, strlen(EXAMPLE_BOARD), "r");
        if (!fp) { perror("fmemopen"); return 1; }

        if (parse_board(&b, fp) != 0)
        {
            fprintf(stderr, "Failed to parse example.\n");
            fclose(fp);
            return 1;
        }
        fclose(fp);
    }
    else
    {
        if (parse_board(&b, stdin) != 0)
        {
            fprintf(stderr, "Failed to parse board from stdin.\n");
            return 1;
        }
    }

    printf("Initial:\n");
    print_board(&b);
    printf("\n");

    solve(&b);

    solve_stats_t stats = analyze(&b);
    print_result(&b, stats);

    return 0;
}

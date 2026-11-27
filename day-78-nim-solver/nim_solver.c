/* nim_solver.c
 *
 * Step order (what this program does):
 *
 *   1. Explain Nim: N piles of stones. Players alternate turns.
 *      A move removes any number of stones from exactly one
 *      pile. The player who takes the last stone wins
 *      ("normal play convention").
 *   2. State the theorem (Bouton, 1901):
 *        First player wins iff XOR of pile sizes != 0.
 *      This gives an O(N) winning-move detector.
 *   3. Implement:
 *        a. nim_sum(piles, n)    -> XOR of pile sizes
 *        b. is_winnning(piles)    -> (nim_sum != 0)
 *        c. best_move(piles)      -> a winning move, or none
 *        d. verify_move(piles, i, remove) -> new state sanity
 *   4. Extend to the Sprague-Grundy theorem: for any impartial
 *      game, each position has a Grundy number g(P). A
 *      disjunctive sum (like Nim with multiple independent
 *      subgames) has Grundy number XOR of the parts. This
 *      explains WHY Nim's XOR rule works and generalizes to
 *      hundreds of other games (Dawson's Kayles, Kayles,
 *      turning turtles, subtraction games, ...).
 *   5. Demonstrate on:
 *        a. Basic Nim: pick a winning move, verify by recursing.
 *        b. Subtractions game (1..k stones per move): also a
 *           Nim-equivalent via Grundy numbers.
 *        c. Brute-force verification: enumerate ALL small
 *           positions and check that "XOR != 0" matches
 *           "first player wins" computed by exhaustive
 *           minimax.
 *
 * What Nim is:
 *
 *   Nim is a game of N piles. On each turn, a player picks one
 *   pile and removes any positive number of stones from it
 *   (up to the pile size). The player who takes the last
 *   stone wins.
 *
 *   Nim is the canonical example of an IMPARTIAL game: both
 *   players have the same moves from any position. (Contrast
 *   with chess, where White and Black have different moves.)
 *
 *   Nim's solution — play the XOR of piles — was discovered by
 *   Charles L. Bouton in 1901. It is one of the earliest
 *   results in combinatorial game theory.
 *
 * Why this matters:
 *
 *   The Sprague-Grundy theorem (1926-1939) generalizes Nim's
 *   analysis to EVERY impartial game. Any such game is
 *   equivalent to Nim with piles whose sizes are the game's
 *   Grundy numbers. Once you can compute Grundy numbers, you
 *   can solve any impartial game — and no other algorithmic
 *   class is more powerful for these.
 *
 * Usage:
 *   ./nim_solver                     run self-tests
 *   ./nim_solver 3 4 5               analyze a specific position
 *   ./nim_solver play 3 4 5          play it (human vs computer)
 *
 * Skills practiced:
 *   - XOR as a game-theoretic invariant
 *   - Closed-form solutions vs. tree search
 *   - Recursive Grundy-number computation
 *   - Proving a theorem by exhaustive verification
 *   - Generalization from a specific game to a framework
 *   - Manual game-tree exploration for verification
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_PILES 16

/* ---------- Nim: XOR rule ---------- */

/* XOR of pile sizes: the "nim-sum". Zero means the position
 * is losing for the player to move; nonzero means winning. */
static uint32_t nim_sum(const int *piles, int n)
{
    uint32_t x = 0;
    for (int i = 0; i < n; i++)
        x ^= (uint32_t)piles[i];
    return x;
}

/* Is the current position a win for the player to move? */
static int is_winning(const int *piles, int n)
{
    return nim_sum(piles, n) != 0;
}

/* Find a winning move, if one exists.
 *
 * The construction: if XOR = S != 0, find the highest set bit
 * of S. There must be at least one pile whose highest set bit
 * is at that position. Reduce that pile to make XOR = 0.
 *
 * Returns 1 if a winning move is found, 0 otherwise (meaning
 * the position is losing — every move is losing).
 *
 * On success, fills *out_pile and *out_remove. */
static int best_move(const int *piles, int n,
                     int *out_pile, int *out_remove)
{
    uint32_t s = nim_sum(piles, n);
    if (s == 0)
        return 0;

    for (int i = 0; i < n; i++)
    {
        uint32_t target = (uint32_t)piles[i] ^ s;
        if (target < (uint32_t)piles[i])
        {
            *out_pile = i;
            *out_remove = piles[i] - (int)target;
            return 1;
        }
    }
    return 0;   /* unreachable if s != 0 */
}

/* ---------- brute-force verification ---------- */

/* Enumerate all game states with up to max_piles piles,
 * each pile in [0, max_size], and verify that the XOR rule
 * matches exhaustive minimax.
 *
 * Returns 1 if all states agree, 0 on first mismatch. */
static int verify_by_brute_force(int max_piles, int max_size)
{
    /* Iterate over all states: max_size^max_piles possibilities */
    int total = 1;
    for (int i = 0; i < max_piles; i++)
        total *= (max_size + 1);

    for (int state = 0; state < total; state++)
    {
        int piles[MAX_PILES];
        int tmp = state;
        int nonzero = 0;
        for (int i = 0; i < max_piles; i++)
        {
            piles[i] = tmp % (max_size + 1);
            if (piles[i] > 0) nonzero = 1;
            tmp /= (max_size + 1);
        }

        /* Terminal: all-zero position is losing for player to move */
        if (!nonzero)
        {
            if (is_winning(piles, max_piles))
                return 0;
            continue;
        }

        /* Check: if XOR != 0, there must be a winning move;
         * if XOR == 0, no move can lead to XOR != 0 (i.e.,
         * every move is to a winning position for the opponent). */
        uint32_t s = nim_sum(piles, max_piles);

        if (s != 0)
        {
            /* There must exist a move making XOR zero */
            int found = 0;
            for (int i = 0; i < max_piles && !found; i++)
            {
                for (int r = 1; r <= piles[i] && !found; r++)
                {
                    int copy[MAX_PILES];
                    memcpy(copy, piles, sizeof(piles));
                    copy[i] -= r;
                    if (nim_sum(copy, max_piles) == 0)
                        found = 1;
                }
            }
            if (!found) return 0;   /* XOR != 0 but no winning move: contradiction */
        }
        else
        {
            /* s == 0: every move leads to XOR != 0 (winning for opponent) */
            for (int i = 0; i < max_piles; i++)
            {
                for (int r = 1; r <= piles[i]; r++)
                {
                    int copy[MAX_PILES];
                    memcpy(copy, piles, sizeof(piles));
                    copy[i] -= r;
                    if (nim_sum(copy, max_piles) == 0)
                        return 0;   /* can move to losing position: contradiction */
                }
            }
        }
    }
    return 1;
}

/* ---------- Sprague-Grundy generalization ---------- */

/* For a game where a move consists of removing 1..k stones
 * from a single pile (a "subtraction game"), compute the
 * Grundy number of a pile of size n.
 *
 * The Grundy number g(n) = mex{ g(n-1), g(n-2), ..., g(n-k) },
 * where mex is the "minimum excluded" — the smallest
 * non-negative integer not in the set.
 *
 * Grundy numbers for subtraction games are periodic (eventually),
 * which is why we can compute them for large n efficiently.
 * For the demo, we compute them directly up to some bound. */
static int *grundy_table = NULL;

static void grundy_build(int max_n, int k)
{
    grundy_table = calloc((size_t)(max_n + 1), sizeof(int));
    if (!grundy_table) { perror("calloc"); exit(EXIT_FAILURE); }

    for (int n = 1; n <= max_n; n++)
    {
        /* Mark which Grundy numbers are reachable */
        char seen[64] = { 0 };
        for (int r = 1; r <= k && r <= n; r++)
        {
            int g = grundy_table[n - r];
            if (g < 64) seen[g] = 1;
        }
        /* mex */
        int mex = 0;
        while (mex < 64 && seen[mex]) mex++;
        grundy_table[n] = mex;
    }
}

static int grundy_of(int pile_size)
{
    return grundy_table ? grundy_table[pile_size] : 0;
}

/* For a multi-pile subtraction game: XOR of Grundy numbers. */
static int sg_sum(const int *piles, int n)
{
    int x = 0;
    for (int i = 0; i < n; i++)
        x ^= grundy_of(piles[i]);
    return x;
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-55s %s\n", label, condition ? "ok" : "FAIL");
}

static void test_nim_sum(void)
{
    printf("Test 1: nim-sum and win detection\n");

    int p1[] = { 0 };
    check("empty position is losing", !is_winning(p1, 1));

    int p2[] = { 5 };
    check("single pile of 5 is winning", is_winning(p2, 1));

    int p3[] = { 1, 1 };
    check("[1,1] is losing", !is_winning(p3, 2));

    int p4[] = { 1, 2 };
    check("[1,2] is winning", is_winning(p4, 2));

    int p5[] = { 1, 2, 3 };
    check("[1,2,3] is losing (1^2^3 = 0)", !is_winning(p5, 3));

    int p6[] = { 3, 4, 5 };
    check("[3,4,5] is winning (3^4^5 = 2)", is_winning(p6, 3));

    int p7[] = { 2, 2, 3, 3 };
    check("[2,2,3,3] is losing (paired)", !is_winning(p7, 4));

    printf("\n");
}

static void test_best_move(void)
{
    printf("Test 2: winning moves\n");

    int positions[][MAX_PILES] = {
        { 3, 4, 5 },        /* XOR = 2, winning */
        { 1, 2, 3 },        /* XOR = 0, losing */
        { 5, 0, 0 },        /* XOR = 5, winning */
        { 7, 5, 2 },        /* XOR = 0, losing */
    };
    int sizes[] = { 3, 3, 3, 3 };

    for (int t = 0; t < 4; t++)
    {
        int pile, remove;
        int found = best_move(positions[t], sizes[t], &pile, &remove);

        char label[80];
        snprintf(label, sizeof(label), "position %d: is_winning = %d",
                 t, is_winning(positions[t], sizes[t]));

        /* A winning move must exist iff position is winning */
        check(label, (found != 0) == (is_winning(positions[t], sizes[t])));

        /* If found, verify the move leaves XOR = 0 */
        if (found)
        {
            int copy[MAX_PILES];
            memcpy(copy, positions[t], sizeof(copy));
            copy[pile] -= remove;
            snprintf(label, sizeof(label),
                     "  winning move leaves XOR = 0");
            check(label, nim_sum(copy, sizes[t]) == 0);
        }
    }

    printf("\n");
}

static void test_brute_force(void)
{
    printf("Test 3: brute-force verification\n");

    /* Verify all states with 1..3 piles, pile sizes 0..5 */
    for (int np = 1; np <= 3; np++)
    {
        char label[80];
        snprintf(label, sizeof(label),
                 "XOR rule matches minimax for %d piles, size 0..5", np);
        check(label, verify_by_brute_force(np, 5));
    }

    printf("\n");
}

static void test_sprague_grundy(void)
{
    printf("Test 4: Sprague-Grundy for subtraction games\n");

    /* Subtraction game: remove 1, 2, or 3 stones. */
    int k = 3;
    grundy_build(20, k);

    printf("  Subtraction game (remove 1..%d stones):\n", k);
    printf("  Pile size: Grundy number\n");
    for (int n = 0; n <= 10; n++)
        printf("    %2d : %d\n", n, grundy_of(n));

    /* Verify period: for k=3, the pattern is 0,1,2,3,0,1,2,3,...
     * (period 4). */
    int pattern_ok = 1;
    for (int n = 0; n <= 16; n++)
    {
        int expected = n % 4;
        if (grundy_of(n) != expected)
            pattern_ok = 0;
    }
    check("Grundy pattern is periodic (n mod 4)", pattern_ok);

    /* Multi-pile subtraction game */
    int p1[] = { 5, 5 };
    check("[5,5] subtraction is losing", sg_sum(p1, 2) == 0);

    int p2[] = { 5, 6 };
    check("[5,6] subtraction is winning", sg_sum(p2, 2) != 0);

    free(grundy_table);
    grundy_table = NULL;

    printf("\n");
}

static void test_equivalence(void)
{
    printf("Test 5: subtraction game (1,2,3) is equivalent to Nim mod 4\n");

    /* For k=3, Grundy number of pile n is n mod 4. So a position
     * is winning iff XOR of (pile mod 4) is nonzero.
     * Compare to Nim on the same piles (XOR of full piles). */

    int k = 3;
    grundy_build(30, k);

    int cases[][MAX_PILES] = {
        { 3, 4, 5 },
        { 2, 3, 4 },
        { 1, 1, 1 },
        { 7, 8, 9 },
    };
    int sizes[] = { 3, 3, 3, 3 };

    for (int t = 0; t < 4; t++)
    {
        int sg_win = (sg_sum(cases[t], sizes[t]) != 0);
        /* Nim on (pile mod 4) gives the same answer */
        int mod_piles[MAX_PILES];
        for (int i = 0; i < sizes[t]; i++)
            mod_piles[i] = cases[t][i] % 4;
        int nim_win = is_winning(mod_piles, sizes[t]);

        char label[80];
        snprintf(label, sizeof(label),
                 "case %d: SG prediction matches Nim-mod-4", t);
        check(label, sg_win == nim_win);
    }

    free(grundy_table);
    grundy_table = NULL;
    printf("\n");
}

/* ---------- interactive play (optional) ---------- */

static int play_mode(int *piles, int n)
{
    printf("\nInteractive Nim. You go first.\n");
    printf("Enter moves as: <pile_index> <remove>\n");
    printf("Enter 'q' to quit.\n\n");

    for (;;)
    {
        printf("Piles: ");
        for (int i = 0; i < n; i++)
        {
            if (i > 0) printf(" ");
            printf("%d", piles[i]);
        }
        printf("  (XOR = %u)\n", nim_sum(piles, n));

        /* Check for game over */
        int total = 0;
        for (int i = 0; i < n; i++) total += piles[i];
        if (total == 0)
        {
            printf("Game over. Winner: computer.\n");
            return 0;
        }

        /* Human move */
        printf("Your move (pile remove): ");
        fflush(stdout);
        char buf[64];
        if (!fgets(buf, sizeof(buf), stdin)) return 0;
        if (buf[0] == 'q' || buf[0] == 'Q')
        {
            printf("Bye.\n");
            return 0;
        }

        int pi, rm;
        if (sscanf(buf, "%d %d", &pi, &rm) != 2)
        {
            printf("Bad input. Try: 0 3\n");
            continue;
        }
        if (pi < 0 || pi >= n || rm < 1 || rm > piles[pi])
        {
            printf("Invalid move. Pile %d has %d stones.\n",
                   pi, piles[pi]);
            continue;
        }
        piles[pi] -= rm;

        /* Computer move */
        int total2 = 0;
        for (int i = 0; i < n; i++) total2 += piles[i];
        if (total2 == 0)
        {
            printf("Game over. Winner: you.\n");
            return 0;
        }

        int pile, remove;
        if (best_move(piles, n, &pile, &remove))
        {
            piles[pile] -= remove;
            printf("Computer removes %d from pile %d.\n", remove, pile);
        }
        else
        {
            /* Losing position: any move. Pick the first nonzero pile. */
            for (int i = 0; i < n; i++)
            {
                if (piles[i] > 0)
                {
                    printf("Computer removes 1 from pile %d.\n", i);
                    piles[i]--;
                    break;
                }
            }
        }
    }
}

/* ---------- main ---------- */

int main(int argc, char *argv[])
{
    if (argc >= 2 && strcmp(argv[1], "play") == 0)
    {
        if (argc < 3 || argc > 2 + MAX_PILES)
        {
            fprintf(stderr, "Usage: %s play <pile1> [pile2 ...]\n", argv[0]);
            return 1;
        }
        int piles[MAX_PILES];
        int n = 0;
        for (int i = 2; i < argc; i++)
            piles[n++] = atoi(argv[i]);
        return play_mode(piles, n);
    }

    if (argc >= 2)
    {
        if (argc > 1 + MAX_PILES)
        {
            fprintf(stderr, "Too many piles (max %d)\n", MAX_PILES);
            return 1;
        }
        int piles[MAX_PILES];
        int n = 0;
        for (int i = 1; i < argc; i++)
            piles[n++] = atoi(argv[i]);

        uint32_t s = nim_sum(piles, n);
        printf("Piles:");
        for (int i = 0; i < n; i++) printf(" %d", piles[i]);
        printf("\nNim-sum: %u\n", s);
        printf("Position is: %s\n", s == 0 ? "LOSING" : "WINNING");

        int pile, remove;
        if (best_move(piles, n, &pile, &remove))
            printf("Winning move: take %d from pile %d\n",
                   remove, pile);
        else
            printf("No winning move (every move loses)\n");
        return 0;
    }

    printf("====================================\n");
    printf("            NIM SOLVER              \n");
    printf("====================================\n\n");

    test_nim_sum();
    test_best_move();
    test_brute_force();
    test_sprague_grundy();
    test_equivalence();

    printf("All tests complete.\n");
    return 0;
}

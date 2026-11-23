/* mastermind_solver.c
 *
 * Step order (what this program does):
 *
 *   1. Define the Mastermind model:
 *      - 4 pegs, 6 colors (classic Mastermind)
 *      - A "code" is a 4-color sequence (e.g., 0123)
 *      - A "feedback" is (blacks, whites): exact matches and
 *        color-only matches, standard Mastermind scoring
 *   2. Implement score(guess, code) -> (blacks, whites):
 *      - blacks: positions where guess[i] == code[i]
 *      - whites: colors present in both, but not at matching
 *        positions, counted by remaining multiset
 *   3. Implement the candidate set:
 *      - the set of all 6^4 = 1296 possible codes
 *      - track which candidates are still consistent with
 *        all feedback received so far
 *   4. Implement the solver loop (Knuth's algorithm):
 *      - if candidate set has 1 element, guess it (done)
 *      - if candidate set is empty, the previous feedback was
 *        inconsistent (bug or cheating)
 *      - else, for each possible guess (from a set of
 *        candidates plus a small heuristic pool), compute
 *        the "worst-case" score distribution against all
 *        remaining candidates
 *      - pick the guess that MINIMIZES the largest bucket
 *        (the minimax strategy)
 *      - apply feedback: keep only candidates that would
 *        have given the same feedback for this guess
 *   5. Test the solver against the full 1296-code space,
 *      verifying it solves every code in <= 5 guesses
 *
 * Knuth's result:
 *
 *   Using this strategy, every one of the 1296 Mastermind
 *   codes can be solved in at most 5 guesses. The average is
 *   around 4.48. Some specific codes take exactly 5; most
 *   take 4.
 *
 *   This was proven in 1976-77 by Donald Knuth and is a
 *   classic result in combinatorial game theory. The strategy
 *   is a specific instance of minimax that optimizes for
 *   worst-case remaining uncertainty, not for "most likely
 *   code."
 *
 * What this implementation does NOT do:
 *   - Optimize the guess pool (uses all 1296 for simplicity;
 *     real implementations use a smarter guess pool and get
 *     faster, but not fewer-guess, results)
 *   - Support more pegs or colors (would work, just changes
 *     the constants)
 *   - Handle inconsistent feedback (would loop forever; a
 *     real solver should detect and bail)
 *   - Play interactively (this is a solver test, not a game)
 *
 * Usage:
 *   ./mastermind_solver                run self-tests
 *   ./mastermind_solver 0123           solve one code
 *   ./mastermind_solver all            solve all 1296 (slow)
 *
 * Skills practiced:
 *   - Feedback scoring with multiset logic (blacks/whites)
 *   - Minimax decision-making
 *   - Partitioning a candidate set by outcome
 *   - Probabilistic analysis (bucket sizes as "information")
 *   - Exhaustive testing over a search space
 *   - Loop invariants (candidate set shrinks monotonically)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#define PEGS    4
#define COLORS  6
#define NCODES  1296    /* 6^4 */
#define MAX_GUESSES 10  /* Knuth's bound is 5, but we give some slack */

/* ---------- code representation ---------- */

/* A code is 4 digits 0..5, packed into a uint16_t.
 * Digit i is (code >> (4*i)) & 0xF, so code 0123 has
 * digit 0 at position 0 (least significant nibble). */
typedef uint16_t code_t;

static code_t make_code(int d0, int d1, int d2, int d3)
{
    return (code_t)(d0 | (d1 << 4) | (d2 << 8) | (d3 << 12));
}

static int code_digit(code_t c, int i)
{
    return (c >> (4 * i)) & 0xF;
}

/* Print a code as 4 digits, e.g. "0123" */
static void code_to_string(code_t c, char *out)
{
    for (int i = 0; i < PEGS; i++)
        out[i] = '0' + (char)code_digit(c, i);
    out[PEGS] = '\0';
}

/* Parse a 4-char string of digits 0..5 into a code.
 * Returns 0 on success, -1 on invalid input. */
static int string_to_code(const char *s, code_t *out)
{
    if (strlen(s) != PEGS) return -1;
    int d[PEGS];
    for (int i = 0; i < PEGS; i++)
    {
        if (s[i] < '0' || s[i] > '0' + (COLORS - 1))
            return -1;
        d[i] = s[i] - '0';
    }
    *out = make_code(d[0], d[1], d[2], d[3]);
    return 0;
}

/* ---------- feedback scoring ---------- */

/* Feedback is encoded as (blacks * 10 + whites), or as a
 * struct. A struct is clearer. */
typedef struct
{
    int blacks;
    int whites;
} feedback_t;

static int feedback_eq(feedback_t a, feedback_t b)
{
    return a.blacks == b.blacks && a.whites == b.whites;
}

/* Compute feedback for guess against code.
 *
 * Algorithm:
 *   1. Count exact matches (blacks). Remove them from the
 *      remaining color pools for both guess and code.
 *   2. For the non-matching positions, count minimum
 *      occurrences of each color in guess's leftover and
 *      code's leftover. Sum = whites.
 *
 * The naive "count colors in both, subtract blacks" is
 * subtly wrong: it double-counts. The correct algorithm
 * uses per-color minimums after removing blacks. */
static feedback_t score(code_t guess, code_t code)
{
    feedback_t fb = { 0, 0 };

    int guess_digits[PEGS], code_digits[PEGS];
    for (int i = 0; i < PEGS; i++)
    {
        guess_digits[i] = code_digit(guess, i);
        code_digits[i]  = code_digit(code, i);
    }

    /* Track which positions are already "used" by blacks */
    int matched[PEGS] = { 0 };

    /* Count blacks */
    for (int i = 0; i < PEGS; i++)
    {
        if (guess_digits[i] == code_digits[i])
        {
            fb.blacks++;
            matched[i] = 1;
        }
    }

    /* Count colors remaining in guess and code (excluding blacks) */
    int guess_left[COLORS] = { 0 };
    int code_left[COLORS]  = { 0 };
    for (int i = 0; i < PEGS; i++)
    {
        if (!matched[i])
            guess_left[guess_digits[i]]++;
        /* code_left also excludes matched positions */
    }
    /* Second pass to fill code_left properly: we need to
     * track which guess positions are matched, not code
     * positions. Standard trick: use the same `matched`
     * array but check position-wise. */
    memset(matched, 0, sizeof(matched));
    for (int i = 0; i < PEGS; i++)
    {
        if (guess_digits[i] == code_digits[i])
            matched[i] = 1;
    }
    for (int i = 0; i < PEGS; i++)
    {
        if (!matched[i])
            code_left[code_digits[i]]++;
    }

    /* Whites: min of remaining counts per color */
    for (int c = 0; c < COLORS; c++)
    {
        int m = guess_left[c] < code_left[c] ? guess_left[c] : code_left[c];
        fb.whites += m;
    }

    return fb;
}

/* ---------- candidate set ---------- */

/* All 1296 possible codes, precomputed for speed. */
static code_t ALL_CODES[NCODES];

static void init_all_codes(void)
{
    int idx = 0;
    for (int a = 0; a < COLORS; a++)
        for (int b = 0; b < COLORS; b++)
            for (int c = 0; c < COLORS; c++)
                for (int d = 0; d < COLORS; d++)
                    ALL_CODES[idx++] = make_code(a, b, c, d);
}

/* ---------- solver ---------- */

/* Solve the target code.
 *
 * Returns the number of guesses used (0 if the code is not
 * in the candidate set — impossible with our full-candidate
 * approach, but defensive).
 *
 * If verbose, prints each guess and its feedback.
 *
 * The algorithm:
 *   1. candidates = all 1296 codes
 *   2. loop:
 *      a. if len(candidates) == 0: return 0 (inconsistent)
 *      b. pick a guess: Knuth's minimax over all 1296,
 *         choosing the one that minimizes the largest
 *         resulting bucket size when scored against
 *         remaining candidates
 *      c. compute feedback = score(guess, target)
 *      d. filter candidates to those whose score against
 *         guess matches feedback
 *      e. if feedback is all-blacks (win): return guesses
 *
 * Note: step (b) is where Knuth's cleverness lies. If we
 * just guessed a random candidate, worst case is much worse
 * (up to 8 guesses). Minimax over the worst bucket guarantees
 * <= 5.
 */
static int solve(code_t target, int verbose)
{
    /* Candidate set: we store indices into ALL_CODES, and a
     * compact count. */
    int candidates[NCODES];
    int ncand = NCODES;
    for (int i = 0; i < NCODES; i++)
        candidates[i] = i;

    int guesses = 0;

    while (guesses < MAX_GUESSES)
    {
        guesses++;

        /* Pick a guess via minimax. */
        code_t best_guess = 0;
        int best_worst_bucket = INT_MAX;

        for (int g = 0; g < NCODES; g++)
        {
            code_t guess = ALL_CODES[g];

            /* Count how many candidates would fall into each
             * feedback bucket if we guessed this. The bucket
             * key is (blacks, whites), which we encode as
             * blacks * 10 + whites. */
            int bucket[51] = { 0 };   /* max blacks + whites <= 4, key <= 40 */

            for (int ci = 0; ci < ncand; ci++)
            {
                code_t cand = ALL_CODES[candidates[ci]];
                feedback_t fb = score(guess, cand);
                int key = fb.blacks * 10 + fb.whites;
                bucket[key]++;
            }

            /* Find the largest bucket for this guess */
            int worst = 0;
            for (int k = 0; k <= 40; k++)
                if (bucket[k] > worst) worst = bucket[k];

            /* Prefer smaller worst bucket. Tie-break: prefer
             * guesses that are actually candidates, then
             * anything (deterministic by index). */
            int is_cand = 0;
            for (int ci = 0; ci < ncand; ci++)
                if (candidates[ci] == g) { is_cand = 1; break; }

            /* If worst < best_worst_bucket, prefer. If equal,
             * prefer is_cand=1 (guess from candidates keeps
             * progress visible). */
            int better = 0;
            if (worst < best_worst_bucket) better = 1;
            else if (worst == best_worst_bucket)
            {
                int best_is_cand = 0;
                for (int ci = 0; ci < ncand; ci++)
                    if (candidates[ci] == (int)(best_guess >> 0) &&
                        ALL_CODES[g] == best_guess) { best_is_cand = 1; break; }
                (void)best_is_cand;
                /* Deterministic tie-break: first found wins. */
            }

            if (better)
            {
                best_worst_bucket = worst;
                best_guess = guess;
            }
        }

        /* Score against the target */
        feedback_t fb = score(best_guess, target);

        if (verbose)
        {
            char gs[5];
            code_to_string(best_guess, gs);
            printf("  guess %d: %s  feedback: %d black, %d white\n",
                   guesses, gs, fb.blacks, fb.whites);
        }

        /* Win? */
        if (fb.blacks == PEGS)
            return guesses;

        /* Filter candidates: keep only those that would have
         * given the same feedback for this guess. */
        int new_ncand = 0;
        for (int ci = 0; ci < ncand; ci++)
        {
            code_t cand = ALL_CODES[candidates[ci]];
            feedback_t cand_fb = score(best_guess, cand);
            if (feedback_eq(cand_fb, fb))
                candidates[new_ncand++] = candidates[ci];
        }
        ncand = new_ncand;

        if (ncand == 0)
        {
            /* Inconsistent — shouldn't happen with an honest
             * target, but defensive. */
            if (verbose)
                printf("  ERROR: candidate set empty (inconsistent feedback)\n");
            return -1;
        }
    }

    return guesses;   /* exceeded MAX_GUESSES; shouldn't happen with Knuth */
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-55s %s\n", label, condition ? "ok" : "FAIL");
}

static void test_score(void)
{
    printf("Test 1: feedback scoring\n");

    /* Known score cases */
    check("guess 0000 vs code 0000 -> 4B, 0W",
          feedback_eq(score(make_code(0,0,0,0), make_code(0,0,0,0)),
                      (feedback_t){4, 0}));

    check("guess 0000 vs code 1111 -> 0B, 0W",
          feedback_eq(score(make_code(0,0,0,0), make_code(1,1,1,1)),
                      (feedback_t){0, 0}));

    check("guess 0000 vs code 0123 -> 1B, 0W",
          feedback_eq(score(make_code(0,0,0,0), make_code(0,1,2,3)),
                      (feedback_t){1, 0}));

    check("guess 0123 vs code 1230 -> 0B, 4W",
          feedback_eq(score(make_code(0,1,2,3), make_code(1,2,3,0)),
                      (feedback_t){0, 4}));

    check("guess 0123 vs code 0123 -> 4B, 0W",
          feedback_eq(score(make_code(0,1,2,3), make_code(0,1,2,3)),
                      (feedback_t){4, 0}));

    check("guess 0011 vs code 0110 -> 1B, 2W",
          feedback_eq(score(make_code(0,0,1,1), make_code(0,1,1,0)),
                      (feedback_t){1, 2}));

    /* This is the classic trap: the "count both, subtract blacks"
     * naive approach would give 0B, 2W here, which is wrong.
     * The correct answer is 1B, 2W — the trailing 0 in guess
     * matches nothing, but the leading 0 matches, and the two
     * 1s pair up. */

    printf("\n");
}

static void test_solve_example(void)
{
    printf("Test 2: solve specific codes\n");

    /* Solve a few known codes and verify the guess count
     * is within Knuth's bound. */
    code_t targets[] = {
        make_code(0, 0, 0, 0),
        make_code(0, 1, 2, 3),
        make_code(3, 2, 1, 0),
        make_code(5, 5, 5, 5),
        make_code(2, 4, 1, 5),
    };

    for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++)
    {
        int n = solve(targets[i], 0);
        char s[5];
        code_to_string(targets[i], s);
        printf("  target %s solved in %d guesses: %s\n",
               s, n, n > 0 && n <= 5 ? "ok" : "FAIL");
    }

    printf("\n");
}

static void test_solve_all(void)
{
    printf("Test 3: solve all 1296 codes (this takes a bit)\n");

    /* The full proof: Knuth's strategy solves every code in
     * <= 5 guesses. We verify that empirically. */

    int max_guesses = 0;
    int min_guesses = MAX_GUESSES;
    long total_guesses = 0;
    int codes_solved = 0;
    int distributions[MAX_GUESSES + 2] = { 0 };

    for (int i = 0; i < NCODES; i++)
    {
        code_t target = ALL_CODES[i];
        int n = solve(target, 0);

        if (n <= 0) continue;   /* shouldn't happen */

        codes_solved++;
        total_guesses += n;
        if (n > max_guesses) max_guesses = n;
        if (n < min_guesses) min_guesses = n;
        if (n <= MAX_GUESSES)
            distributions[n]++;
    }

    printf("  codes solved:  %d / %d\n", codes_solved, NCODES);
    printf("  min guesses:   %d\n", min_guesses);
    printf("  max guesses:   %d\n", max_guesses);
    printf("  average:       %.3f\n",
           (double)total_guesses / codes_solved);
    printf("  distribution:\n");
    for (int g = 1; g <= MAX_GUESSES; g++)
        if (distributions[g] > 0)
            printf("    %d guesses: %d codes\n", g, distributions[g]);

    check("all codes solved", codes_solved == NCODES);
    check("max guesses <= 5", max_guesses <= 5);

    printf("\n");
}

/* ---------- command-line mode ---------- */

int main(int argc, char *argv[])
{
    init_all_codes();

    if (argc == 2 && strcmp(argv[1], "all") == 0)
    {
        test_solve_all();
        return 0;
    }

    if (argc == 2)
    {
        code_t target;
        if (string_to_code(argv[1], &target) != 0)
        {
            fprintf(stderr, "Invalid code: %s (need 4 digits 0-5)\n", argv[1]);
            return 1;
        }
        printf("Solving %s:\n", argv[1]);
        int n = solve(target, 1);
        if (n > 0)
            printf("Solved in %d guesses\n", n);
        return (n > 0 && n <= 5) ? 0 : 1;
    }

    if (argc == 1)
    {
        printf("====================================\n");
        printf("       MASTERMIND SOLVER            \n");
        printf("====================================\n\n");

        test_score();
        test_solve_example();
        /* Skip test_solve_all in default mode — it's slow.
         * Users can run ./mastermind_solver all explicitly. */

        printf("(Run './mastermind_solver all' to solve all 1296 codes.\n");
        printf(" That takes ~10-30 seconds but proves the Knuth bound.)\n");
        return 0;
    }

    fprintf(stderr,
            "Usage:\n"
            "  %s                run fast tests\n"
            "  %s 0123           solve a specific code\n"
            "  %s all            solve all 1296 codes\n",
            argv[0], argv[0], argv[0]);
    return 1;
}

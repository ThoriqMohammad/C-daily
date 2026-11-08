/* poker.c
 *
 * Poker hand evaluator: rank any 5-card poker hand from best
 * (Royal Flush) to worst (High Card), and compare two hands.
 *
 * Hand rankings, from highest to lowest:
 *
 *   1. Royal Flush     A K Q J T, all same suit
 *   2. Straight Flush  five consecutive ranks, same suit
 *   3. Four of a Kind  four cards of the same rank
 *   4. Full House      three of one rank, two of another
 *   5. Flush           five cards of the same suit
 *   6. Straight        five consecutive ranks
 *   7. Three of a Kind three cards of the same rank
 *   8. Two Pair        two pairs
 *   9. One Pair        one pair
 *  10. High Card       none of the above
 *
 * Comparison rules (tiebreakers):
 *   - Higher rank beats lower rank.
 *   - For same rank, compare the cards involved in the ranking:
 *     a pair of Kings beats a pair of Queens, two pair Aces and
 *     Kings beats two pair Aces and Queens, etc.
 *   - Then compare the remaining "kicker" cards in descending order.
 *
 * The program:
 *   - parses hands from compact strings ("As Kd Qh Jc Ts")
 *   - evaluates a hand to a ranked structure
 *   - compares any two hands
 *   - runs test vectors covering every hand category
 *
 * Usage:
 *   ./poker
 *   ./poker "As Kd Qh Jc Ts" "2s 3d 4h 5c 7s"
 *
 * Skills practiced:
 *   - Card representation as bits (rank 2-14, suit 0-3)
 *   - Sorting arrays with custom comparators
 *   - Frequency counting with small fixed arrays
 *   - Straight detection
 *   - Structured lexicographic comparison
 *   - Test-driven development with known cases
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------- hand ranks ---------- */

typedef enum
{
    HIGH_CARD = 0,
    ONE_PAIR,
    TWO_PAIR,
    THREE_OF_A_KIND,
    STRAIGHT,
    FLUSH,
    FULL_HOUSE,
    FOUR_OF_A_KIND,
    STRAIGHT_FLUSH,
    ROYAL_FLUSH
} hand_rank_t;

/* A card is 0..51: rank*4 + suit.
 * Ranks: 0=2, 1=3, ..., 8=T, 9=J, 10=Q, 11=K, 12=A
 * Suits: 0=clubs, 1=diamonds, 2=hearts, 3=spades */
typedef int card_t;

#define CARD_RANK(c) ((c) / 4)     /* 0..12 */
#define CARD_SUIT(c) ((c) % 4)     /* 0..3  */

/* Evaluated hand: rank plus up to 5 tiebreaker values */
typedef struct
{
    hand_rank_t rank;
    int         tie[5];    /* descending significance */
} evaluated_hand;

/* ---------- card parsing and printing ---------- */

/* Parse a rank character to 0..12 */
static int parse_rank(char c)
{
    switch (toupper((unsigned char)c))
    {
        case '2': return 0;
        case '3': return 1;
        case '4': return 2;
        case '5': return 3;
        case '6': return 4;
        case '7': return 5;
        case '8': return 6;
        case '9': return 7;
        case 'T': return 8;
        case 'J': return 9;
        case 'Q': return 10;
        case 'K': return 11;
        case 'A': return 12;
        default:  return -1;
    }
}

/* Parse a suit character to 0..3 */
static int parse_suit(char c)
{
    switch (tolower((unsigned char)c))
    {
        case 'c': return 0;
        case 'd': return 1;
        case 'h': return 2;
        case 's': return 3;
        default:  return -1;
    }
}

/* Parse a single card like "As" or "Td" */
static card_t parse_card(const char *s)
{
    if (s == NULL || s[0] == '\0' || s[1] == '\0')
        return -1;
    int r = parse_rank(s[0]);
    int u = parse_suit(s[1]);
    if (r < 0 || u < 0)
        return -1;
    return r * 4 + u;
}

/* Parse a hand string like "As Kd Qh Jc Ts" */
static int parse_hand(const char *s, card_t *out)
{
    int count = 0;
    const char *p = s;

    while (*p && count < 5)
    {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t' || *p == ',')
            p++;
        if (*p == '\0')
            break;

        card_t c = parse_card(p);
        if (c < 0)
            return -1;

        out[count++] = c;
        p += 2;   /* rank + suit */
    }

    return (count == 5) ? 0 : -1;
}

/* Print a card like "As" */
static void print_card(card_t c)
{
    static const char rank_chars[] = "23456789TJQKA";
    static const char suit_chars[] = "cdhs";
    printf("%c%c", rank_chars[CARD_RANK(c)], suit_chars[CARD_SUIT(c)]);
}

/* Print an evaluated hand as a human-readable category */
static const char *rank_name(hand_rank_t r)
{
    switch (r)
    {
        case ROYAL_FLUSH:      return "Royal Flush";
        case STRAIGHT_FLUSH:   return "Straight Flush";
        case FOUR_OF_A_KIND:   return "Four of a Kind";
        case FULL_HOUSE:       return "Full House";
        case FLUSH:            return "Flush";
        case STRAIGHT:         return "Straight";
        case THREE_OF_A_KIND:  return "Three of a Kind";
        case TWO_PAIR:         return "Two Pair";
        case ONE_PAIR:         return "One Pair";
        case HIGH_CARD:        return "High Card";
    }
    return "?";
}

/* ---------- hand evaluation ---------- */

/* Comparator for sorting ranks in descending order */
static int cmp_desc(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

/* Comparator for sorting ranks in ascending order */
static int cmp_asc(const void *a, const void *b)
{
    return *(const int *)b - *(const int *)a;
}

/* Evaluate a 5-card hand into a rank plus tiebreakers */
static void evaluate(const card_t *hand, evaluated_hand *out)
{
    int rank_counts[13] = {0};   /* how many of each rank     */
    int suit_counts[4]  = {0};   /* how many of each suit     */
    int ranks[5];
    int flush_suit = -1;

    /* Tally */
    for (int i = 0; i < 5; i++)
    {
        int r = CARD_RANK(hand[i]);
        int s = CARD_SUIT(hand[i]);
        rank_counts[r]++;
        suit_counts[s]++;
        ranks[i] = r;
    }

    /* Is this a flush? (all 5 same suit) */
    for (int s = 0; s < 4; s++)
    {
        if (suit_counts[s] == 5)
        {
            flush_suit = s;
            break;
        }
    }

    /* Sort ranks descending for tiebreakers */
    qsort(ranks, 5, sizeof(int), cmp_desc);

    /* Is this a straight? (consecutive ranks) */
    /* Special case: A-2-3-4-5 is a straight ("wheel") with
     * ace playing low. Detect by looking for the specific
     * pattern {12, 3, 2, 1, 0}. */
    int is_wheel = (ranks[0] == 12 && ranks[1] == 3 &&
                    ranks[2] == 2  && ranks[3] == 1 && ranks[4] == 0);

    int is_straight = is_wheel ||
        (ranks[0] - ranks[1] == 1 &&
         ranks[1] - ranks[2] == 1 &&
         ranks[2] - ranks[3] == 1 &&
         ranks[3] - ranks[4] == 1);

    /* Group ranks by count */
    /* order[0] = rank with highest count, etc. */
    int order[5];
    int order_count = 0;

    /* First pass: count 4s, 3s, 2s, 1s in priority */
    for (int count = 4; count >= 1; count--)
    {
        for (int r = 12; r >= 0; r--)
        {
            if (rank_counts[r] == count)
                order[order_count++] = r;
        }
    }

    int four = (order_count > 0 && rank_counts[order[0]] == 4);
    int three = (order_count > 0 && rank_counts[order[0]] == 3);
    int pair1 = 0, pair2 = 0;
    int pairs = 0;
    for (int r = 12; r >= 0; r--)
    {
        if (rank_counts[r] == 2)
        {
            if (pairs == 0) pair1 = r;
            else if (pairs == 1) pair2 = r;
            pairs++;
        }
    }

    /* Highest card (for wheel, the "5" is the high card) */
    int high = is_wheel ? 3 : ranks[0];

    /* Now classify */
    if (is_straight && flush_suit >= 0)
    {
        if (high == 12)
        {
            out->rank = ROYAL_FLUSH;
            out->tie[0] = 12;
        }
        else
        {
            out->rank = STRAIGHT_FLUSH;
            out->tie[0] = high;
        }
        return;
    }

    if (four)
    {
        out->rank = FOUR_OF_A_KIND;
        out->tie[0] = order[0];                /* quad rank */
        /* Kicker: highest non-quad rank */
        for (int i = 0; i < order_count; i++)
            if (rank_counts[order[i]] == 1)
            {
                out->tie[1] = order[i];
                break;
            }
        return;
    }

    if (three && pairs == 1)
    {
        out->rank = FULL_HOUSE;
        out->tie[0] = order[0];                /* triplet rank */
        out->tie[1] = pair1;                   /* pair rank    */
        return;
    }

    if (flush_suit >= 0)
    {
        out->rank = FLUSH;
        for (int i = 0; i < 5; i++)
            out->tie[i] = ranks[i];            /* descending  */
        return;
    }

    if (is_straight)
    {
        out->rank = STRAIGHT;
        out->tie[0] = high;
        return;
    }

    if (three)
    {
        out->rank = THREE_OF_A_KIND;
        out->tie[0] = order[0];                /* trip rank */
        /* Two kickers */
        int kicker = 1;
        for (int i = 0; i < order_count && kicker < 3; i++)
            if (rank_counts[order[i]] == 1)
                out->tie[kicker++] = order[i];
        return;
    }

    if (pairs == 2)
    {
        out->rank = TWO_PAIR;
        out->tie[0] = pair1;                   /* higher pair */
        out->tie[1] = pair2;                   /* lower pair  */
        for (int r = 12; r >= 0; r--)
            if (rank_counts[r] == 1)
            {
                out->tie[2] = r;               /* kicker */
                break;
            }
        return;
    }

    if (pairs == 1)
    {
        out->rank = ONE_PAIR;
        out->tie[0] = pair1;
        int kicker = 1;
        for (int r = 12; r >= 0 && kicker < 4; r--)
            if (rank_counts[r] == 1)
                out->tie[kicker++] = r;
        return;
    }

    /* High card */
    out->rank = HIGH_CARD;
    for (int i = 0; i < 5; i++)
        out->tie[i] = ranks[i];
}

/* Compare two evaluated hands. Returns:
 *   >0 if a beats b
 *   <0 if b beats a
 *    0 if tie
 */
static int compare_hands(const evaluated_hand *a, const evaluated_hand *b)
{
    if (a->rank != b->rank)
        return (int)a->rank - (int)b->rank;

    for (int i = 0; i < 5; i++)
        if (a->tie[i] != b->tie[i])
            return a->tie[i] - b->tie[i];

    return 0;
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-55s %s\n", label, condition ? "ok" : "FAIL");
}

/* Helper: parse and evaluate a hand from a string */
static evaluated_hand eval_string(const char *s)
{
    card_t hand[5];
    evaluated_hand out;
    memset(&out, 0, sizeof(out));
    if (parse_hand(s, hand) != 0)
    {
        fprintf(stderr, "Bad hand: %s\n", s);
        exit(EXIT_FAILURE);
    }
    evaluate(hand, &out);
    return out;
}

/* Helper: check that a hand string has the expected category */
static int assert_category(const char *s, hand_rank_t expected)
{
    evaluated_hand h = eval_string(s);
    if (h.rank != expected)
    {
        printf("    %s: got %s, expected %s\n",
               s, rank_name(h.rank), rank_name(expected));
        return 0;
    }
    return 1;
}

static void test_categories(void)
{
    printf("Test 1: every hand category is detected\n");

    check("Royal Flush: As Ks Qs Js Ts",
          assert_category("As Ks Qs Js Ts", ROYAL_FLUSH));
    check("Straight Flush: 9h 8h 7h 6h 5h",
          assert_category("9h 8h 7h 6h 5h", STRAIGHT_FLUSH));
    check("Four of a Kind: 7s 7h 7d 7c 2s",
          assert_category("7s 7h 7d 7c 2s", FOUR_OF_A_KIND));
    check("Full House: Ks Kh Kd 4c 4s",
          assert_category("Ks Kh Kd 4c 4s", FULL_HOUSE));
    check("Flush: As Js 9s 6s 3s",
          assert_category("As Js 9s 6s 3s", FLUSH));
    check("Straight: 9h 8s 7d 6c 5h",
          assert_category("9h 8s 7d 6c 5h", STRAIGHT));
    check("Three of a Kind: Qs Qh Qd 8c 3s",
          assert_category("Qs Qh Qd 8c 3s", THREE_OF_A_KIND));
    check("Two Pair: Js Jh 4d 4c As",
          assert_category("Js Jh 4d 4c As", TWO_PAIR));
    check("One Pair: 8s 8h Kd 4c 2s",
          assert_category("8s 8h Kd 4c 2s", ONE_PAIR));
    check("High Card: As Jd 8h 6c 2s",
          assert_category("As Jd 8h 6c 2s", HIGH_CARD));

    printf("\n");
}

static void test_wheel(void)
{
    printf("Test 2: the wheel (A-2-3-4-5) is a straight\n");

    /* Wheel is a straight (5-high), not an ace-high straight */
    check("As 2h 3d 4c 5s is a straight",
          assert_category("As 2h 3d 4c 5s", STRAIGHT));

    /* Wheel straight should lose to 6-high straight */
    evaluated_hand wheel = eval_string("As 2h 3d 4c 5s");
    evaluated_hand six_high = eval_string("6h 5s 4d 3c 2h");
    check("wheel < 6-high straight",
          compare_hands(&wheel, &six_high) < 0);

    /* Wheel straight flush */
    check("As 2s 3s 4s 5s is a straight flush",
          assert_category("As 2s 3s 4s 5s", STRAIGHT_FLUSH));

    printf("\n");
}

static void test_comparisons(void)
{
    printf("Test 3: comparisons between categories\n");

    evaluated_hand high_card  = eval_string("As Jd 8h 6c 2s");
    evaluated_hand one_pair   = eval_string("8s 8h Kd 4c 2s");
    evaluated_hand two_pair   = eval_string("Js Jh 4d 4c As");
    evaluated_hand trips      = eval_string("Qs Qh Qd 8c 3s");
    evaluated_hand straight   = eval_string("9h 8s 7d 6c 5h");
    evaluated_hand flush      = eval_string("As Js 9s 6s 3s");
    evaluated_hand full_house = eval_string("Ks Kh Kd 4c 4s");
    evaluated_hand quads      = eval_string("7s 7h 7d 7c 2s");
    evaluated_hand s_flush    = eval_string("9h 8h 7h 6h 5h");
    evaluated_hand royal      = eval_string("As Ks Qs Js Ts");

    /* Chain: high < pair < two pair < trips < straight < flush
     *        < full house < quads < s.flush < royal */
    check("pair > high card",  compare_hands(&one_pair, &high_card) > 0);
    check("two pair > pair",   compare_hands(&two_pair, &one_pair) > 0);
    check("trips > two pair",  compare_hands(&trips, &two_pair) > 0);
    check("straight > trips",  compare_hands(&straight, &trips) > 0);
    check("flush > straight",  compare_hands(&flush, &straight) > 0);
    check("full house > flush", compare_hands(&full_house, &flush) > 0);
    check("quads > full house", compare_hands(&quads, &full_house) > 0);
    check("straight flush > quads", compare_hands(&s_flush, &quads) > 0);
    check("royal > straight flush", compare_hands(&royal, &s_flush) > 0);

    printf("\n");
}

static void test_tiebreakers(void)
{
    printf("Test 4: tiebreakers within a category\n");

    /* Two pairs: Aces and Kings beats Aces and Queens */
    evaluated_hand ak = eval_string("As Ah Kd Kc 2s");
    evaluated_hand aq = eval_string("As Ah Qd Qc 2s");
    check("two pair AAKK > AAQQ", compare_hands(&ak, &aq) > 0);

    /* Pair with different kickers */
    evaluated_hand kk_a = eval_string("Ks Kh As 4c 2s");
    evaluated_hand kk_q = eval_string("Ks Kh Qs 4c 2s");
    check("pair KK with A kicker > with Q kicker",
          compare_hands(&kk_a, &kk_q) > 0);

    /* High card comparison */
    evaluated_hand akj = eval_string("As Kd Jh 6c 2s");
    evaluated_hand aqj = eval_string("As Qd Jh 6c 2s");
    check("high card AK > high card AQ",
          compare_hands(&akj, &aqj) > 0);

    /* Same hand should tie */
    evaluated_hand same1 = eval_string("As Kd Jh 6c 2s");
    evaluated_hand same2 = eval_string("Ac Kh Js 6d 2h");
    check("identical ranks but different suits tie",
          compare_hands(&same1, &same2) == 0);

    printf("\n");
}

/* ---------- command-line mode ---------- */

static void print_hand_and_eval(const char *s)
{
    card_t hand[5];
    if (parse_hand(s, hand) != 0)
    {
        printf("Invalid hand: %s\n", s);
        return;
    }

    printf("Hand: ");
    for (int i = 0; i < 5; i++)
    {
        if (i > 0) printf(" ");
        print_card(hand[i]);
    }

    evaluated_hand e;
    evaluate(hand, &e);
    printf("  ->  %s\n", rank_name(e.rank));
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        printf("====================================\n");
        printf("       POKER HAND EVALUATOR         \n");
        printf("====================================\n\n");

        test_categories();
        test_wheel();
        test_comparisons();
        test_tiebreakers();

        printf("All tests complete.\n");
        return 0;
    }

    if (argc == 2)
    {
        print_hand_and_eval(argv[1]);
        return 0;
    }

    if (argc == 3)
    {
        card_t a[5], b[5];
        if (parse_hand(argv[1], a) != 0 ||
            parse_hand(argv[2], b) != 0)
        {
            fprintf(stderr, "Invalid hand(s).\n");
            return 1;
        }

        evaluated_hand ea, eb;
        evaluate(a, &ea);
        evaluate(b, &eb);

        printf("Hand A: ");
        for (int i = 0; i < 5; i++) { if (i) printf(" "); print_card(a[i]); }
        printf("  (%s)\n", rank_name(ea.rank));

        printf("Hand B: ");
        for (int i = 0; i < 5; i++) { if (i) printf(" "); print_card(b[i]); }
        printf("  (%s)\n", rank_name(eb.rank));

        int cmp = compare_hands(&ea, &eb);
        if (cmp > 0)      printf("\nHand A wins.\n");
        else if (cmp < 0) printf("\nHand B wins.\n");
        else              printf("\nTie.\n");

        return 0;
    }

    fprintf(stderr,
            "Usage:\n"
            "  %s                          run self-tests\n"
            "  %s \"As Kd Qh Jc Ts\"       evaluate one hand\n"
            "  %s \"...\" \"...\"            compare two hands\n",
            argv[0], argv[0], argv[0]);
    return 1;
}

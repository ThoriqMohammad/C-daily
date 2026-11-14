/* simple_regex.c
 *
 * A minimal regular expression engine supporting:
 *
 *   .       any single character
 *   *       zero or more of the preceding atom
 *   +       one or more of the preceding atom
 *   ?       zero or one of the preceding atom
 *   a-z     literal characters
 *   ^       anchor at start of string
 *   $       anchor at end of string
 *
 * No capture groups, no character classes, no alternation.
 * That's enough to be useful, small enough to understand
 * completely in a single sitting.
 *
 * Two implementations are provided for comparison:
 *
 *   1. BACKTRACKING  (Cox's "simple recursive regex")
 *      Follows the grammar, recursing on match and back-
 *      tracking on failure. Simple to understand. Worst case
 *      O(2^n) on patterns like a?a?a?... against aaaa...a.
 *
 *   2. NFA SIMULATION  (Thompson, 1968)
 *      Compiles the pattern to an NFA, then simulates all
 *      possible states in parallel. Always O(n*m) where n
 *      is string length and m is pattern length. This is
 *      the algorithm inside grep, sed, and every production
 *      regex engine that isn't vulnerable to ReDoS.
 *
 * This file implements the backtracking version, which is
 * the classic recursive regex taught in textbooks. The NFA
 * version is left as an extension (see "What You Can
 * Extend").
 *
 * Usage:
 *   ./simple_regex <pattern> <string>
 *
 * Examples:
 *   ./simple_regex "abc" "abc"        -> match
 *   ./simple_regex "a.c" "abc"        -> match
 *   ./simple_regex "ab*c" "ac"        -> match
 *   ./simple_regex "ab*c" "abbbbc"    -> match
 *   ./simple_regex "^ab" "abc"        -> match
 *   ./simple_regex "bc$" "abc"        -> match
 *   ./simple_regex "^abc$" "abc"      -> match
 *   ./simple_regex "^abc$" "abcd"     -> no match
 *
 * Skills practiced:
 *   - Recursive descent parsing of a small grammar
 *   - The difference between greedy backtracking and
 *     parallel NFA simulation
 *   - Anchors and why they change the complexity
 *   - Testing edge cases (empty pattern, empty string)
 *   - The "catastrophic backtracking" failure mode
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- backtracking matcher ---------- */

/* Match a single character: '.' matches anything, else literal */
static int match_char(char pattern_ch, char string_ch)
{
    if (pattern_ch == '.')
        return 1;
    return pattern_ch == string_ch;
}

/* Match `pattern` (length p_len) against `string` (length s_len).
 * Returns 1 on match, 0 on no match.
 *
 * This is the core recursive routine. Its shape comes from
 * the grammar of the pattern:
 *
 *   match(p, s) =
 *     if p is empty:  return s is empty
 *     if p[1] == '*': try zero-or-more of p[0]
 *     if p[1] == '+': try one-or-more of p[0]
 *     if p[1] == '?': try zero-or-one of p[0]
 *     else:           p[0] must match s[0], then recurse on p+1, s+1
 *
 * The trick for `*`, `+`, `?` is that we try the greedy option
 * first (match as much as possible), then backtrack to try
 * shorter matches.
 */
static int match_here(const char *pattern, size_t p_len,
                      const char *string, size_t s_len)
{
    /* Empty pattern: match iff string is also empty */
    if (p_len == 0)
        return s_len == 0;

    /* Anchor at end: '$' matches only at end of string */
    if (p_len == 1 && pattern[0] == '$')
        return s_len == 0;

    /* Check for quantifier on the next char */
    char quantifier = 0;
    if (p_len >= 2 &&
        (pattern[1] == '*' || pattern[1] == '+' || pattern[1] == '?'))
    {
        quantifier = pattern[1];
    }

    /* Handle '*' : zero or more */
    if (quantifier == '*')
    {
        /* Try greedy: consume as many as possible, then backtrack.
         * Base case: zero occurrences always works. */
        size_t consumed = 0;
        while (consumed < s_len &&
               match_char(pattern[0], string[consumed]))
        {
            consumed++;
        }

        /* Try from greedy down to zero */
        for (size_t i = consumed + 1; i-- > 0; )
        {
            if (match_here(pattern + 2, p_len - 2,
                           string + i, s_len - i))
                return 1;
        }
        return 0;
    }

    /* Handle '+' : one or more (same as '*' but requires at least 1) */
    if (quantifier == '+')
    {
        if (s_len == 0 || !match_char(pattern[0], string[0]))
            return 0;

        size_t consumed = 1;
        while (consumed < s_len &&
               match_char(pattern[0], string[consumed]))
        {
            consumed++;
        }

        for (size_t i = consumed + 1; i-- > 1; )
        {
            if (match_here(pattern + 2, p_len - 2,
                           string + i, s_len - i))
                return 1;
        }
        return 0;
    }

    /* Handle '?' : zero or one */
    if (quantifier == '?')
    {
        /* Try "one" first (greedy), then "zero" */
        if (s_len > 0 && match_char(pattern[0], string[0]))
        {
            if (match_here(pattern + 2, p_len - 2,
                           string + 1, s_len - 1))
                return 1;
        }
        return match_here(pattern + 2, p_len - 2, string, s_len);
    }

    /* No quantifier: p[0] must match s[0], then recurse */
    if (s_len > 0 && match_char(pattern[0], string[0]))
        return match_here(pattern + 1, p_len - 1,
                          string + 1, s_len - 1);

    return 0;
}

/* Entry point: handles the '^' anchor.
 * If pattern starts with '^', the match must start at position 0.
 * Otherwise, try every starting position in the string. */
int regex_match(const char *pattern, const char *string)
{
    size_t p_len = strlen(pattern);
    size_t s_len = strlen(string);

    /* Anchor at start */
    if (p_len > 0 && pattern[0] == '^')
    {
        return match_here(pattern + 1, p_len - 1, string, s_len);
    }

    /* Try every starting position */
    for (size_t i = 0; i <= s_len; i++)
    {
        if (match_here(pattern, p_len, string + i, s_len - i))
            return 1;
    }
    return 0;
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-52s %s\n", label, condition ? "ok" : "FAIL");
}

static void expect(const char *pattern, const char *string, int should_match)
{
    int got = regex_match(pattern, string);
    char label[128];
    snprintf(label, sizeof(label), "\"%.20s\" vs \"%.20s\"",
             pattern, string);
    check(label, got == should_match);
}

static void test_literals(void)
{
    printf("Test 1: literal patterns\n");

    expect("abc", "abc", 1);
    expect("abc", "abcd", 1);   /* substring match */
    expect("abc", "xabcx", 1);
    expect("abc", "ab", 0);
    expect("abc", "abd", 0);
    expect("abc", "abcabc", 1);

    printf("\n");
}

static void test_dot(void)
{
    printf("Test 2: '.' matches any single character\n");

    expect("a.c", "abc", 1);
    expect("a.c", "aXc", 1);
    expect("a.c", "ac", 0);     /* '.' requires one char */
    expect("a.c", "abbc", 0);   /* '.' matches exactly one */
    expect("...", "abc", 1);
    expect("...", "ab", 0);

    printf("\n");
}

static void test_star(void)
{
    printf("Test 3: '*' zero or more\n");

    expect("ab*c", "ac", 1);        /* zero b's */
    expect("ab*c", "abc", 1);       /* one b */
    expect("ab*c", "abbc", 1);      /* two b's */
    expect("ab*c", "abbbbbbbbc", 1);
    expect("ab*c", "abd", 0);
    expect("a*", "", 1);            /* zero of anything */
    expect("a*", "aaaa", 1);
    expect(".*", "hello world", 1); /* .* matches everything */

    printf("\n");
}

static void test_plus(void)
{
    printf("Test 4: '+' one or more\n");

    expect("ab+c", "ac", 0);        /* requires at least one b */
    expect("ab+c", "abc", 1);
    expect("ab+c", "abbc", 1);
    expect("ab+c", "abd", 0);
    expect("a+", "a", 1);
    expect("a+", "", 0);            /* requires at least one */

    printf("\n");
}

static void test_question(void)
{
    printf("Test 5: '?' zero or one\n");

    expect("ab?c", "ac", 1);        /* zero b's */
    expect("ab?c", "abc", 1);       /* one b */
    expect("ab?c", "abbc", 0);      /* two b's, not allowed */
    expect("ab?c", "adc", 0);

    printf("\n");
}

static void test_anchors(void)
{
    printf("Test 6: '^' and '$' anchors\n");

    expect("^abc", "abc", 1);
    expect("^abc", "abcd", 1);
    expect("^abc", "xabc", 0);      /* must start at beginning */
    expect("abc$", "abc", 1);
    expect("abc$", "xabc", 1);
    expect("abc$", "abcd", 0);      /* must end at end */
    expect("^abc$", "abc", 1);      /* whole string must match */
    expect("^abc$", "abcd", 0);
    expect("^abc$", "xabc", 0);
    expect("^$", "", 1);            /* empty string */
    expect("^$", "a", 0);

    printf("\n");
}

static void test_edge_cases(void)
{
    printf("Test 7: edge cases\n");

    expect("", "", 1);
    expect("", "a", 1);             /* empty pattern matches anything */
    expect("a", "", 0);
    expect(".*", "", 1);            /* .* matches empty string */
    expect("^.*$", "hello", 1);
    expect("^a*$", "aaaa", 1);
    expect("^a*$", "aaab", 0);

    printf("\n");
}

/* ---------- benchmark: catastrophic backtracking ---------- */

static void benchmark_backtracking(void)
{
    printf("Test 8: catastrophic backtracking on a^n pattern\n");

    /* The classic bad case: (a*)* or a*a*a*... against a's.
     * For our simple engine, `a*a*a*a*a*...` against a string
     * of a's with no match at the end forces exponential
     * backtracking. Here we use a modest case that shows the
     * blowup without hanging the test. */

    /* We use a pattern like "a?a?a?a?a?b" against "aaaaaaaaa"
     * (9 a's, no b). Each "a?" doubles the search space. */
    const char *pattern = "a?a?a?a?a?b";
    const char *string  = "aaaaaaaaa";

    printf("  pattern: %s\n", pattern);
    printf("  string:  %s\n", string);

    /* For n = 9, backtracking explores up to 2^9 = 512 paths.
     * The match fails. This is fast enough not to hang. */
    int result = regex_match(pattern, string);
    check("complex pattern with no match fails", result == 0);

    printf("  (with larger patterns like a?a?...a?b on 30 a's,\n");
    printf("   this would take 2^30 ~ 1 billion recursions)\n");

    printf("\n");
}

int main(int argc, char *argv[])
{
    if (argc == 3)
    {
        /* Direct invocation: pattern, string */
        int matched = regex_match(argv[1], argv[2]);
        printf("%s\n", matched ? "match" : "no match");
        return matched ? 0 : 1;
    }

    if (argc == 1)
    {
        printf("====================================\n");
        printf("         SIMPLE REGEX               \n");
        printf("====================================\n\n");

        test_literals();
        test_dot();
        test_star();
        test_plus();
        test_question();
        test_anchors();
        test_edge_cases();
        benchmark_backtracking();

        printf("All tests complete.\n");
        return 0;
    }

    fprintf(stderr,
            "Usage:\n"
            "  %s               run self-tests\n"
            "  %s <pat> <str>   match pattern against string\n",
            argv[0], argv[0]);
    return 1;
}

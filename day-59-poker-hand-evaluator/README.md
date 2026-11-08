# Day 59 — poker_hand_evaluator

Rank any 5-card poker hand and compare two hands, using the
standard poker hand hierarchy.

## The 10 Hand Categories

From highest to lowest:

| # | Category | Example | Description |
|---|----------|---------|-------------|
| 1 | Royal Flush | A K Q J T, same suit | Highest straight flush |
| 2 | Straight Flush | 9 8 7 6 5, same suit | Five consecutive, same suit |
| 3 | Four of a Kind | 7 7 7 7 2 | Four same rank |
| 4 | Full House | K K K 4 4 | Three of one rank, two of another |
| 5 | Flush | A J 9 6 3, same suit | Five same suit |
| 6 | Straight | 9 8 7 6 5 | Five consecutive ranks |
| 7 | Three of a Kind | Q Q Q 8 3 | Three same rank |
| 8 | Two Pair | J J 4 4 A | Two pairs |
| 9 | One Pair | 8 8 K 4 2 | One pair |
| 10 | High Card | A J 8 6 2 | None of the above |

## How Evaluation Works

For any 5-card hand, compute:

1. **`rank_counts[13]`** — how many of each rank (2 through Ace)
2. **`suit_counts[4]`** — how many of each suit
3. **`ranks[5]`** — sorted descending
4. **`is_straight`** — are the ranks consecutive?
5. **`is_flush`** — is one `suit_counts[i] == 5`?

Then classify top-down:

    if is_straight and is_flush:
        if high == Ace:
            return ROYAL_FLUSH
        else:
            return STRAIGHT_FLUSH
    if any rank has count 4:
        return FOUR_OF_A_KIND
    if any rank has count 3 AND some rank has count 2:
        return FULL_HOUSE
    if is_flush:
        return FLUSH
    if is_straight:
        return STRAIGHT
    if any rank has count 3:
        return THREE_OF_A_KIND
    if two ranks have count 2:
        return TWO_PAIR
    if any rank has count 2:
        return ONE_PAIR
    return HIGH_CARD

Order matters. A straight flush is both a straight and a flush;
we check the "and" first. A full house is both trips and a pair;
we check the "and" before checking either alone.

## Tiebreakers

Comparing two hands is a **lexicographic** comparison of:

1. **Category rank** (higher wins outright)
2. **Tiebreaker values**, in descending order

For example, two hands in the "One Pair" category:

    Hand A: K K A 4 2
    Hand B: K K Q 4 2

Both are one pair. The pair rank is K in both. So we compare
kickers: A vs Q → A wins → Hand A wins.

This requires storing up to **5 tiebreaker values** per hand,
sorted in the order they matter for comparison.

| Category | Tiebreakers |
|----------|-------------|
| Royal Flush | (none — only one possible) |
| Straight Flush | high card of the straight |
| Four of a Kind | quad rank, then kicker |
| Full House | triplet rank, then pair rank |
| Flush | all five cards descending |
| Straight | high card of the straight |
| Three of a Kind | trip rank, then 2 kickers |
| Two Pair | higher pair, lower pair, kicker |
| One Pair | pair rank, then 3 kickers |
| High Card | all five cards descending |

## The Wheel (A-2-3-4-5)

The "wheel" straight — `A 2 3 4 5` — is an edge case. The Ace
is normally the highest card (rank 12), but in a straight it
can also play low. `A 2 3 4 5` is a valid straight, and its
high card is **5**, not Ace.

Detect it by checking for the specific sorted pattern:

    ranks[0] == 12 && ranks[1] == 3 &&
    ranks[2] == 2  && ranks[3] == 1 && ranks[4] == 0

Then the "high card" is the 5 (rank value 3). A wheel straight
loses to a 6-high straight (`6 5 4 3 2`), because `6 > 5`.

The wheel straight flush — `A 2 3 4 5` all same suit — is the
**lowest** straight flush, not a royal.

## Usage

    ./poker                            run self-tests
    ./poker "As Kd Qh Jc Ts"           evaluate one hand
    ./poker "As Kd Qh Jc Ts" "2s 3d 4h 5c 7s"   compare two hands

Cards are `rank + suit`:

- **Ranks**: `2 3 4 5 6 7 8 9 T J Q K A`
- **Suits**: `c` (clubs), `d` (diamonds), `h` (hearts), `s` (spades)

The `T` for ten is standard poker notation.

## Example

    $ ./poker "As Ks Qs Js Ts"
    Hand: As Ks Qs Js Ts  ->  Royal Flush

    $ ./poker "As Kd Qh Jc Ts" "2s 3d 4h 5c 7s"
    Hand A: As Kd Qh Jc Ts  (Straight)
    Hand B: 2s 3d 4h 5c 7s  (High Card)

    Hand A wins.

## Build

    gcc -Wall -Wextra -std=c99 -o poker poker.c

## Skills Practiced

- **Card representation** as small integers (`rank*4 + suit`)
- **Frequency counting** with fixed-size arrays
- **Straight detection** with the wheel special case
- **Structured lexicographic comparison**
- **Test-driven development** with known cases
- **Parsing compact string formats** into typed data
- **Enums for categorized output**

## Why the Tests Matter

Test 1 verifies all 10 categories are detected on known hands.
If the classification order is wrong (e.g., checking flush before
checking four-of-a-kind), a hand like `As Ah Ad Ac Ks` might be
misclassified — but the test would catch it.

Test 2 is the wheel. It's the most common poker evaluator bug:
a naive `is_straight` check on `{12, 3, 2, 1, 0}` returns false,
because `12 - 3 != 1`. So wheel hands are misclassified as High
Card. Handling the wheel explicitly is what makes the code
correct.

Test 3 checks the full ranking chain — every adjacent pair of
categories should compare correctly.

Test 4 checks tiebreakers within each category. Two pairs with
different high pairs, one pair with different kickers, identical
ranks but different suits (should tie).

## Notes

- **Only 5-card hands.** Real poker variants (Texas Hold'em,
  Omaha, 7-card stud) require evaluating the best 5-card hand
  out of 7 or more cards. That's 21 combinations for 7-card
  Hold'em — nontrivial but doable.
- **Suits are only compared for flush detection.** Two hands
  with identical ranks and different suits are a tie. If you
  wanted to break ties by suit (some rule sets do), you'd need
  to add suit ranking, which most evaluators don't.
- **No wild cards, no jokers.** Standard 52-card deck only.
- **Order of category checks matters.** The code checks
  `is_straight && is_flush` before `is_flush`. If you reversed
  the order, a straight flush would be misclassified as a flush.
  Similarly, full house is checked before either trips or pair
  individually.
- **`qsort` for ranks.** Insertion sort on 5 elements is
  actually faster, but qsort is idiomatic and portable.
- **`evaluated_hand.tie[]` is zero-initialized.** Only the
  positions used by the current category are filled; the rest
  stay 0. `compare_hands` walks all 5 positions, so unused
  positions compare as 0 vs 0 — no effect on the result.
- **The `parse_hand` function is strict.** It requires exactly
  5 cards; no more, no fewer. The parser also accepts `,` as a
  separator, so `"As,Kd,Qh,Jc,Ts"` works too.
- **Aces are 12, not 14.** This keeps the array indexed 0..12,
  so `rank_counts[13]` is enough. The tradeoff: the wheel
  detection has to special-case `ranks[0] == 12`.

## What You Can Extend

- **Best-of-7 evaluation** — enumerate all C(7,5)=21 subsets
  and take the max
- **Texas Hold'em full** — combine player's 2 hole cards with
  5 community cards, evaluate 21 combinations
- **Hand strength calculation** — enumerate remaining cards,
  compute win probability
- **Suits as tiebreakers** — some variants break exact ties by
  highest suit
- **Low poker (deuce-to-seven)** — invert rankings so lowest
  hand wins
- **Wild cards / jokers** — every Joker can be any card; test
  all substitutions
- **Monte Carlo simulation** — deal random hands, compute
  win rates for different starting hands

## Why This Matters as a Portfolio Piece

- **A correct poker evaluator** is a real, testable, self-contained
  achievement. Unlike some toy programs, "correct" has a
  precise definition that can be verified against the rules.
- **The edge cases matter.** The wheel (`A-2-3-4-5`) is the
  famous gotcha. Handling it shows care for correctness.
- **The comparison structure generalizes.** Lexicographic
  comparison with tiebreakers is the same mechanism as
  comparing version strings, timestamps with multiple
  resolution levels, or any multi-field key sort.
- **Test-driven development.** You can see the assertions
  in the tests, and they're the same ones a poker expert
  would write. That's a strong signal.

## Why This Matters for Programming

- **Bit-packing and integer encoding** — representing a card
  as a single integer is a small-scale version of how data
  is packed throughout systems code.
- **Frequency counting** — the `rank_counts`/`suit_counts`
  arrays are a template for histogram-style analysis used
  everywhere from text processing to image processing.
- **Comparison function design** — the lexicographic pattern
  (`if a[i] != b[i] return a[i] - b[i]`) is the foundation of
  sorting, deduplication, and lookup by composite key.
- **The pitfall of order-of-checks** — putting `if (is_flush)`
  before `if (is_straight && is_flush)` is a classic bug.
  Understanding that categories can overlap, and that the
  more specific case must be checked first, is a broadly
  applicable design principle.

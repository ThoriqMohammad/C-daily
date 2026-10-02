/* wordle_clone.c
 *
 * A simple Wordle clone for the terminal.
 *
 * The computer picks a 5-letter secret word.
 * The player has 6 attempts to guess it.
 *
 * After each guess, each letter is marked:
 *   [G] Green  — correct letter, correct position
 *   [Y] Yellow — correct letter, wrong position
 *   [.] Gray   — letter not in the word
 *
 * Usage:
 *   ./wordle_clone
 *
 * Skills practiced:
 *   - Strings
 *   - Arrays
 *   - Loops and conditionals
 *   - Randomness
 *   - Game state
 *   - Input validation
 *   - Frequency counting (handling duplicates)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>

#define WORD_LEN 5
#define MAX_GUESSES 6

static const char *WORDS[] = {
    "apple", "brave", "crane", "drive", "eagle",
    "flame", "grape", "house", "input", "joker",
    "knife", "lemon", "music", "noble", "ocean",
    "piano", "queen", "radio", "stone", "tiger",
    "union", "voice", "water", "xenon", "yield",
    "zebra", "adopt", "bloom", "charm", "drift"
};
#define NUM_WORDS (sizeof(WORDS) / sizeof(WORDS[0]))

/* Result of one letter */
typedef enum {
    GRAY,    /* not in the word */
    YELLOW,  /* in the word, wrong position */
    GREEN    /* in the word, correct position */
} LetterResult;

/* Read a line of exactly 5 letters */
bool read_guess(char *guess)
{
    char line[100];

    printf("Enter your guess: ");

    if (fgets(line, sizeof(line), stdin) == NULL)
        return false;

    /* Strip newline */
    line[strcspn(line, "\n")] = '\0';

    /* Validate length */
    if (strlen(line) != WORD_LEN)
    {
        printf("Please enter exactly %d letters.\n\n", WORD_LEN);
        return false;
    }

    /* Validate letters and convert to lowercase */
    for (int i = 0; i < WORD_LEN; i++)
    {
        if (!isalpha((unsigned char) line[i]))
        {
            printf("Only letters are allowed.\n\n");
            return false;
        }
        guess[i] = (char) tolower((unsigned char) line[i]);
    }
    guess[WORD_LEN] = '\0';

    return true;
}

/* Evaluate the guess against the secret word.
 * Handles duplicate letters correctly.
 */
void evaluate(const char *secret, const char *guess,
              LetterResult result[WORD_LEN])
{
    int freq[26] = {0};

    /* First pass: mark greens, count remaining letters */
    for (int i = 0; i < WORD_LEN; i++)
    {
        if (guess[i] == secret[i])
        {
            result[i] = GREEN;
        }
        else
        {
            result[i] = GRAY;
            freq[secret[i] - 'a']++;
        }
    }

    /* Second pass: mark yellows */
    for (int i = 0; i < WORD_LEN; i++)
    {
        if (result[i] == GRAY && freq[guess[i] - 'a'] > 0)
        {
            result[i] = YELLOW;
            freq[guess[i] - 'a']--;
        }
    }
}

/* Print the guess with color markers */
void print_result(const char *guess,
                  const LetterResult result[WORD_LEN])
{
    for (int i = 0; i < WORD_LEN; i++)
    {
        const char *marker =
            result[i] == GREEN  ? "G" :
            result[i] == YELLOW ? "Y" : ".";

        printf("  %c [%s]", guess[i], marker);
    }
    printf("\n");
}

int main(void)
{
    srand((unsigned) time(NULL));

    const char *secret = WORDS[rand() % NUM_WORDS];

    printf("====================================\n");
    printf("           WORDLE CLONE             \n");
    printf("====================================\n\n");

    printf("Guess the %d-letter word.\n", WORD_LEN);
    printf("You have %d attempts.\n\n", MAX_GUESSES);
    printf("Markers:\n");
    printf("  G = correct letter, correct position\n");
    printf("  Y = correct letter, wrong position\n");
    printf("  . = letter not in the word\n\n");

    for (int attempt = 1; attempt <= MAX_GUESSES; attempt++)
    {
        char guess[WORD_LEN + 1];
        LetterResult result[WORD_LEN];

        printf("Attempt %d/%d\n", attempt, MAX_GUESSES);

        if (!read_guess(guess))
        {
            attempt--;
            continue;
        }

        evaluate(secret, guess, result);
        print_result(guess, result);
        printf("\n");

        /* Check for win */
        bool won = true;
        for (int i = 0; i < WORD_LEN; i++)
        {
            if (result[i] != GREEN)
            {
                won = false;
                break;
            }
        }

        if (won)
        {
            printf("🎉 You won in %d attempt(s)!\n", attempt);
            return 0;
        }
    }

    printf("😢 Out of attempts. The word was: %s\n", secret);
    return 0;
}

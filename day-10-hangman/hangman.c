/* hangman.c
 *
 * A simple Hangman game.
 *
 * The computer picks a random word.
 * The player guesses letters one at a time.
 * The player has 6 wrong guesses before the man is hanged.
 *
 * Usage:
 *   ./hangman
 *
 * Skills practiced:
 *   - Strings and character arrays
 *   - Loops and conditionals
 *   - Randomness (rand, srand)
 *   - Game state tracking
 *   - Input validation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_WRONG 6

/* Word list */
static const char *WORDS[] = {
    "programming",
    "computer",
    "keyboard",
    "algorithm",
    "compiler",
    "function",
    "variable",
    "pointer",
    "network",
    "security"
};

#define NUM_WORDS (sizeof(WORDS) / sizeof(WORDS[0]))

/* Display the hangman based on wrong guesses */
void draw_hangman(int wrong)
{
    const char *stages[] = {
        "  +---+\n  |   |\n      |\n      |\n      |\n      |\n=========",
        "  +---+\n  |   |\n  O   |\n      |\n      |\n      |\n=========",
        "  +---+\n  |   |\n  O   |\n  |   |\n      |\n      |\n=========",
        "  +---+\n  |   |\n  O   |\n /|   |\n      |\n      |\n=========",
        "  +---+\n  |   |\n  O   |\n /|\\  |\n      |\n      |\n=========",
        "  +---+\n  |   |\n  O   |\n /|\\  |\n /    |\n      |\n=========",
        "  +---+\n  |   |\n  O   |\n /|\\  |\n / \\  |\n      |\n========="
    };

    printf("%s\n\n", stages[wrong]);
}

/* Display the word with guessed letters revealed */
void display_word(const char *word, const int guessed[26])
{
    for (int i = 0; word[i] != '\0'; i++) {
        char c = tolower(word[i]);
        if (guessed[c - 'a'])
            printf("%c ", word[i]);
        else
            printf("_ ");
    }
    printf("\n");
}

/* Check if the word is fully guessed */
int word_complete(const char *word, const int guessed[26])
{
    for (int i = 0; word[i] != '\0'; i++) {
        char c = tolower(word[i]);
        if (!guessed[c - 'a'])
            return 0;
    }
    return 1;
}

int main(void)
{
    char play_again;

    srand((unsigned) time(NULL));

    printf("====================================\n");
    printf("            HANGMAN                 \n");
    printf("   You have %d wrong guesses.       \n", MAX_WRONG);
    printf("====================================\n\n");

    do {
        const char *word = WORDS[rand() % NUM_WORDS];
        int guessed[26] = {0};
        int wrong = 0;
        char input[100];

        while (wrong < MAX_WRONG && !word_complete(word, guessed)) {
            draw_hangman(wrong);
            printf("Word: ");
            display_word(word, guessed);
            printf("Wrong guesses: %d/%d\n", wrong, MAX_WRONG);
            printf("Guess a letter: ");

            if (scanf("%99s", input) != 1) {
                while (getchar() != '\n');
                continue;
            }

            /* Clear remaining input */
            while (getchar() != '\n');

            if (strlen(input) != 1 || !isalpha((unsigned char) input[0])) {
                printf("Please enter a single letter.\n\n");
                continue;
            }

            char c = tolower(input[0]);

            if (guessed[c - 'a']) {
                printf("You already guessed '%c'.\n\n", c);
                continue;
            }

            guessed[c - 'a'] = 1;

            if (strchr(word, c) == NULL)
                wrong++;
        }

        /* Final state */
        if (word_complete(word, guessed)) {
            printf("\n🎉 You won! The word was: %s\n", word);
        } else {
            draw_hangman(wrong);
            printf("\n😢 You lost. The word was: %s\n", word);
        }

        printf("\nPlay again? (y/n): ");
        scanf(" %c", &play_again);
        while (getchar() != '\n');
        printf("\n");

    } while (play_again == 'y' || play_again == 'Y');

    printf("Thanks for playing!\n");
    return 0;
}

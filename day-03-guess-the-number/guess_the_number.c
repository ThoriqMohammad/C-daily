/* guess_the_number.c
 *
 * A simple number-guessing game.
 * The program picks a random number between 1 and 100.
 * The player has 7 attempts to guess it.
 *
 * After each guess, the program says "too high" or "too low".
 * If the player guesses correctly, the number of attempts is shown.
 * If the player runs out of attempts, the secret number is revealed.
 *
 * Usage:
 *   ./guess_the_number
 *
 * Skills practiced:
 *   - rand() and srand()
 *   - Loops and conditionals
 *   - User input with scanf()
 *   - Game state tracking
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_ATTEMPTS 7
#define MIN_NUMBER   1
#define MAX_NUMBER   100

int main(void)
{
    int secret;
    int guess;
    int attempts = 0;
    int won = 0;

    /* Seed the random number generator with the current time */
    srand((unsigned) time(NULL));

    /* Pick a random number between MIN_NUMBER and MAX_NUMBER */
    secret = rand() % (MAX_NUMBER - MIN_NUMBER + 1) + MIN_NUMBER;

    printf("====================================\n");
    printf("   GUESS THE NUMBER (1 to 100)      \n");
    printf("   You have %d attempts.           \n", MAX_ATTEMPTS);
    printf("====================================\n\n");

    while (attempts < MAX_ATTEMPTS)
    {
        printf("Attempt %d/%d — Enter your guess: ", attempts + 1, MAX_ATTEMPTS);

        if (scanf("%d", &guess) != 1)
        {
            printf("Invalid input. Please enter a number.\n");
            /* Clear the input buffer */
            while (getchar() != '\n')
                ;
            continue;
        }

        attempts++;

        if (guess < MIN_NUMBER || guess > MAX_NUMBER)
        {
            printf("Please guess a number between %d and %d.\n\n", MIN_NUMBER, MAX_NUMBER);
            continue;
        }

        if (guess == secret)
        {
            won = 1;
            printf("\n🎉 Correct! You guessed it in %d attempt(s).\n", attempts);
            break;
        }
        else if (guess < secret)
        {
            printf("Too low!\n\n");
        }
        else
        {
            printf("Too high!\n\n");
        }
    }

    if (!won)
    {
        printf("\n😢 Out of attempts! The number was: %d\n", secret);
    }

    printf("\nThanks for playing!\n");
    return 0;
}

/* rock_paper_scissors.c
 *
 * A simple Rock Paper Scissors game against the computer.
 *
 * Rules:
 *   Rock beats Scissors
 *   Scissors beats Paper
 *   Paper beats Rock
 *
 * The player chooses R, P, or S.
 * The computer chooses randomly.
 * The winner is displayed, along with the current score.
 *
 * Usage:
 *   ./rock_paper_scissors
 *
 * Skills practiced:
 *   - rand() and srand()
 *   - Character input and validation
 *   - Game loop with score tracking
 *   - Switch statements
 *   - Enum-like thinking with chars
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

/* Return a random choice: 'R', 'P', or 'S' */
char computer_choice(void)
{
    const char choices[] = {'R', 'P', 'S'};
    return choices[rand() % 3];
}

/* Determine the result: 1 = player wins, -1 = computer wins, 0 = draw */
int judge(char player, char computer)
{
    if (player == computer)
        return 0;

    if ((player == 'R' && computer == 'S') ||
        (player == 'S' && computer == 'P') ||
        (player == 'P' && computer == 'R'))
        return 1;

    return -1;
}

/* Print a human-readable name for a choice */
const char *choice_name(char c)
{
    switch (c)
    {
        case 'R': return "Rock";
        case 'P': return "Paper";
        case 'S': return "Scissors";
        default:  return "Unknown";
    }
}

int main(void)
{
    int player_wins = 0;
    int computer_wins = 0;
    int draws = 0;
    char input;

    srand((unsigned) time(NULL));

    printf("====================================\n");
    printf("       ROCK PAPER SCISSORS          \n");
    printf("   Enter R, P, or S. Q to quit.     \n");
    printf("====================================\n\n");

    while (1)
    {
        printf("Your choice: ");

        if (scanf(" %c", &input) != 1)
            break;

        input = toupper(input);

        if (input == 'Q')
            break;

        if (input != 'R' && input != 'P' && input != 'S')
        {
            printf("Invalid choice. Please enter R, P, or S.\n\n");
            continue;
        }

        char computer = computer_choice();
        int result = judge(input, computer);

        printf("You chose %s. Computer chose %s.\n",
               choice_name(input), choice_name(computer));

        if (result == 1)
        {
            printf("You win this round!\n");
            player_wins++;
        }
        else if (result == -1)
        {
            printf("Computer wins this round.\n");
            computer_wins++;
        }
        else
        {
            printf("It's a draw.\n");
            draws++;
        }

        printf("Score — You: %d | Computer: %d | Draws: %d\n\n",
               player_wins, computer_wins, draws);
    }

    printf("\n====================================\n");
    printf("Final Score\n");
    printf("You:      %d\n", player_wins);
    printf("Computer: %d\n", computer_wins);
    printf("Draws:    %d\n", draws);

    if (player_wins > computer_wins)
        printf("\nYou won the match! 🎉\n");
    else if (computer_wins > player_wins)
        printf("\nComputer won the match. 🤖\n");
    else
        printf("\nThe match is a draw. 🤝\n");

    printf("Thanks for playing!\n");
    return 0;
}

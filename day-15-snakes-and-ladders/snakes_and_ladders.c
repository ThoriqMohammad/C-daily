/* snakes_and_ladders.c
 *
 * A simple simulation of the classic Snakes and Ladders board game.
 *
 * The board is 10x10 (squares 1-100).
 * Players roll a die and move forward.
 * Landing on a ladder sends you up; landing on a snake sends you down.
 *
 * This version is a *simulation*: two players race to square 100,
 * and the program reports the winner and the number of turns.
 *
 * Usage:
 *   ./snakes_and_ladders
 *
 * Skills practiced:
 *   - Arrays (the board)
 *   - Randomness (dice rolls)
 *   - Simulation
 *   - Loops and conditionals
 *   - Expected value (statistics)
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BOARD_SIZE 100
#define NUM_PLAYERS 2
#define MAX_TURNS 10000

/* Ladders: start -> end (move up) */
static const int ladder_start[] = { 1,  4,  9, 21, 28, 36, 51, 71, 80};
static const int ladder_end[]   = {38, 14, 31, 42, 84, 44, 67, 91, 100};

/* Snakes: start -> end (move down) */
static const int snake_start[] = {16, 47, 49, 56, 62, 64, 87, 93, 95, 98};
static const int snake_end[]   = { 6, 26, 11, 53, 19, 60, 24, 73, 75, 78};

#define NUM_LADDERS (sizeof(ladder_start) / sizeof(ladder_start[0]))
#define NUM_SNAKES  (sizeof(snake_start)  / sizeof(snake_start[0]))

/* Build the board: board[i] = destination from square i */
void build_board(int board[])
{
    for (int i = 0; i <= BOARD_SIZE; i++)
        board[i] = i;

    for (size_t i = 0; i < NUM_LADDERS; i++)
        board[ladder_start[i]] = ladder_end[i];

    for (size_t i = 0; i < NUM_SNAKES; i++)
        board[snake_start[i]] = snake_end[i];
}

/* Roll a six-sided die */
int roll_die(void)
{
    return rand() % 6 + 1;
}

/* Move a player and return the new position */
int move_player(int position, int board[])
{
    int roll = roll_die();
    int next = position + roll;

    if (next > BOARD_SIZE)
        return position;   /* bounce back: stay put */

    return board[next];
}

int main(void)
{
    int board[BOARD_SIZE + 1];
    int position[NUM_PLAYERS] = {0};
    int turns = 0;
    int winner = -1;

    srand((unsigned) time(NULL));
    build_board(board);

    printf("====================================\n");
    printf("       SNAKES AND LADDERS           \n");
    printf("     First to square 100 wins.      \n");
    printf("====================================\n\n");

    while (winner == -1 && turns < MAX_TURNS)
    {
        turns++;

        for (int p = 0; p < NUM_PLAYERS; p++)
        {
            int roll = roll_die();
            int next = position[p] + roll;

            if (next > BOARD_SIZE)
                next = position[p];   /* overshoot: stay put */

            position[p] = board[next];

            printf("Turn %d, Player %d: rolled %d, moved to %d\n",
                   turns, p + 1, roll, position[p]);

            if (position[p] == BOARD_SIZE)
            {
                winner = p;
                break;
            }
        }
    }

    printf("\n====================================\n");
    if (winner >= 0)
        printf("Player %d wins in %d turns!\n", winner + 1, turns);
    else
        printf("No winner after %d turns.\n", MAX_TURNS);

    printf("Final positions: Player 1 = %d, Player 2 = %d\n",
           position[0], position[1]);

    return 0;
}

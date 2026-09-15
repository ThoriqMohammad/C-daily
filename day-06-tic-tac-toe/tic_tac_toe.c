/* tic_tac_toe.c
 *
 * A simple Tic-Tac-Toe game: Player vs. Computer.
 *
 * The board is a 3x3 grid, stored as a 2D array.
 * The player is 'X', the computer is 'O'.
 *
 * The player chooses a square (1-9):
 *
 *   1 | 2 | 3
 *  ---+---+---
 *   4 | 5 | 6
 *  ---+---+---
 *   7 | 8 | 9
 *
 * The computer plays randomly on an empty square.
 * The first to get three in a row (row, column, or diagonal) wins.
 * If all 9 squares are filled with no winner, it's a draw.
 *
 * Usage:
 *   ./tic_tac_toe
 *
 * Skills practiced:
 *   - 2D arrays
 *   - Functions
 *   - Game loop
 *   - Win detection
 *   - Input validation
 *   - Randomness (for computer move)
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SIZE 3

/* Print the board */
void print_board(char board[SIZE][SIZE])
{
    printf("\n");
    for (int row = 0; row < SIZE; row++)
    {
        printf("  ");
        for (int col = 0; col < SIZE; col++)
        {
            printf(" %c ", board[row][col]);
            if (col < SIZE - 1)
                printf("|");
        }
        printf("\n");
        if (row < SIZE - 1)
            printf("  ---+---+---\n");
    }
    printf("\n");
}

/* Check if a player has won */
int check_win(char board[SIZE][SIZE], char player)
{
    /* Check rows */
    for (int row = 0; row < SIZE; row++)
    {
        if (board[row][0] == player &&
            board[row][1] == player &&
            board[row][2] == player)
            return 1;
    }

    /* Check columns */
    for (int col = 0; col < SIZE; col++)
    {
        if (board[0][col] == player &&
            board[1][col] == player &&
            board[2][col] == player)
            return 1;
    }

    /* Check diagonals */
    if (board[0][0] == player &&
        board[1][1] == player &&
        board[2][2] == player)
        return 1;

    if (board[0][2] == player &&
        board[1][1] == player &&
        board[2][0] == player)
        return 1;

    return 0;
}

/* Check if the board is full */
int is_full(char board[SIZE][SIZE])
{
    for (int row = 0; row < SIZE; row++)
        for (int col = 0; col < SIZE; col++)
            if (board[row][col] == ' ')
                return 0;
    return 1;
}

/* Convert square number (1-9) to row and column */
void square_to_rowcol(int square, int *row, int *col)
{
    *row = (square - 1) / SIZE;
    *col = (square - 1) % SIZE;
}

/* Player's move */
void player_move(char board[SIZE][SIZE])
{
    int square;

    while (1)
    {
        printf("Your move (1-9): ");
        if (scanf("%d", &square) != 1)
        {
            while (getchar() != '\n')
                ;
            printf("Invalid input. Please enter a number 1-9.\n");
            continue;
        }

        if (square < 1 || square > 9)
        {
            printf("Please enter a number between 1 and 9.\n");
            continue;
        }

        int row, col;
        square_to_rowcol(square, &row, &col);

        if (board[row][col] != ' ')
        {
            printf("That square is already taken. Try again.\n");
            continue;
        }

        board[row][col] = 'X';
        return;
    }
}

/* Computer's move (random empty square) */
void computer_move(char board[SIZE][SIZE])
{
    int empty[9][2];
    int count = 0;

    /* Collect all empty squares */
    for (int row = 0; row < SIZE; row++)
    {
        for (int col = 0; col < SIZE; col++)
        {
            if (board[row][col] == ' ')
            {
                empty[count][0] = row;
                empty[count][1] = col;
                count++;
            }
        }
    }

    if (count == 0)
        return;

    /* Pick a random empty square */
    int pick = rand() % count;
    int row = empty[pick][0];
    int col = empty[pick][1];

    board[row][col] = 'O';
    printf("Computer plays square %d.\n", row * SIZE + col + 1);
}

int main(void)
{
    char board[SIZE][SIZE];
    char play_again;

    srand((unsigned) time(NULL));

    printf("====================================\n");
    printf("          TIC-TAC-TOE               \n");
    printf("     You are X. Computer is O.      \n");
    printf("====================================\n");

    do
    {
        /* Initialize the board */
        for (int row = 0; row < SIZE; row++)
            for (int col = 0; col < SIZE; col++)
                board[row][col] = ' ';

        printf("\nSquares are numbered 1-9:\n");
        print_board(board);

        /* Game loop */
        while (1)
        {
            /* Player's turn */
            player_move(board);
            print_board(board);

            if (check_win(board, 'X'))
            {
                printf("🎉 You win!\n");
                break;
            }

            if (is_full(board))
            {
                printf("It's a draw. 🤝\n");
                break;
            }

            /* Computer's turn */
            computer_move(board);
            print_board(board);

            if (check_win(board, 'O'))
            {
                printf("🤖 Computer wins!\n");
                break;
            }

            if (is_full(board))
            {
                printf("It's a draw. 🤝\n");
                break;
            }
        }

        printf("\nPlay again? (y/n): ");
        scanf(" %c", &play_again);

    } while (play_again == 'y' || play_again == 'Y');

    printf("Thanks for playing!\n");
    return 0;
}

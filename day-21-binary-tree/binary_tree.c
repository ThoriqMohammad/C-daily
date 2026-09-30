/* binary_tree.c
 *
 * A binary search tree (BST) implementation in C.
 *
 * A BST is a binary tree where:
 *   - The left subtree contains only values less than the node.
 *   - The right subtree contains only values greater than the node.
 *
 * Supports:
 *   - Insert
 *   - Search
 *   - In-order traversal (sorted output)
 *   - Pre-order traversal
 *   - Post-order traversal
 *   - Minimum / maximum
 *   - Height
 *   - Count
 *   - Free
 *
 * Usage:
 *   ./binary_tree
 *
 * Skills practiced:
 *   - Structs
 *   - Recursion
 *   - Pointers
 *   - Dynamic memory (malloc, free)
 *   - Tree algorithms
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* A node in the BST */
typedef struct Node {
    int data;
    struct Node *left;
    struct Node *right;
} Node;

/* Create a new node */
Node *create_node(int value)
{
    Node *n = malloc(sizeof(Node));
    if (n == NULL)
    {
        fprintf(stderr, "Error: malloc failed.\n");
        exit(EXIT_FAILURE);
    }
    n->data = value;
    n->left = NULL;
    n->right = NULL;
    return n;
}

/* Insert a value into the tree */
Node *insert(Node *root, int value)
{
    if (root == NULL)
        return create_node(value);

    if (value < root->data)
        root->left = insert(root->left, value);
    else if (value > root->data)
        root->right = insert(root->right, value);
    /* Duplicates are ignored */

    return root;
}

/* Search for a value */
bool search(Node *root, int value)
{
    if (root == NULL)
        return false;

    if (value == root->data)
        return true;
    else if (value < root->data)
        return search(root->left, value);
    else
        return search(root->right, value);
}

/* In-order traversal: prints sorted order */
void inorder(Node *root)
{
    if (root == NULL)
        return;

    inorder(root->left);
    printf("%d ", root->data);
    inorder(root->right);
}

/* Pre-order traversal */
void preorder(Node *root)
{
    if (root == NULL)
        return;

    printf("%d ", root->data);
    preorder(root->left);
    preorder(root->right);
}

/* Post-order traversal */
void postorder(Node *root)
{
    if (root == NULL)
        return;

    postorder(root->left);
    postorder(root->right);
    printf("%d ", root->data);
}

/* Find the minimum value */
int minimum(Node *root)
{
    while (root->left != NULL)
        root = root->left;
    return root->data;
}

/* Find the maximum value */
int maximum(Node *root)
{
    while (root->right != NULL)
        root = root->right;
    return root->data;
}

/* Compute the height of the tree */
int height(Node *root)
{
    if (root == NULL)
        return -1;

    int left_height = height(root->left);
    int right_height = height(root->right);

    return 1 + (left_height > right_height ? left_height : right_height);
}

/* Count the nodes */
int count_nodes(Node *root)
{
    if (root == NULL)
        return 0;

    return 1 + count_nodes(root->left) + count_nodes(root->right);
}

/* Free the entire tree */
void free_tree(Node *root)
{
    if (root == NULL)
        return;

    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

int main(void)
{
    Node *root = NULL;

    int values[] = {50, 30, 70, 20, 40, 60, 80, 10, 25, 35, 45, 55, 65, 75, 85};
    int n = sizeof(values) / sizeof(values[0]);

    printf("====================================\n");
    printf("        BINARY SEARCH TREE          \n");
    printf("====================================\n\n");

    /* Insert values */
    printf("Inserting: ");
    for (int i = 0; i < n; i++)
    {
        printf("%d ", values[i]);
        root = insert(root, values[i]);
    }
    printf("\n\n");

    /* Traversals */
    printf("In-order   (sorted): ");
    inorder(root);
    printf("\n");

    printf("Pre-order  (root first): ");
    preorder(root);
    printf("\n");

    printf("Post-order (root last):  ");
    postorder(root);
    printf("\n\n");

    /* Properties */
    printf("Minimum:  %d\n", minimum(root));
    printf("Maximum:  %d\n", maximum(root));
    printf("Height:   %d\n", height(root));
    printf("Count:    %d\n", count_nodes(root));

    /* Search */
    printf("\nSearching:\n");
    int targets[] = {40, 100, 10, 999};
    for (int i = 0; i < 4; i++)
    {
        printf("  %3d: %s\n", targets[i],
               search(root, targets[i]) ? "found" : "not found");
    }

    /* Clean up */
    free_tree(root);

    return 0;
}

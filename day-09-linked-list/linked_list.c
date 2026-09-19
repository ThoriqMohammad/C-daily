/* linked_list.c
 *
 * A singly linked list implementation in C.
 *
 * Supports:
 *   - insert at head
 *   - insert at tail
 *   - delete by value
 *   - search
 *   - print
 *   - free all
 *
 * Usage:
 *   ./linked_list
 *
 * Skills practiced:
 *   - struct
 *   - malloc / free
 *   - pointers and pointer-to-pointer
 *   - dynamic data structures
 *   - memory management
 */

#include <stdio.h>
#include <stdlib.h>

/* A node in the linked list */
typedef struct Node {
    int data;
    struct Node *next;
} Node;

/* Create a new node with the given value */
Node *create_node(int value)
{
    Node *n = malloc(sizeof(Node));
    if (n == NULL) {
        fprintf(stderr, "Error: malloc failed\n");
        exit(EXIT_FAILURE);
    }
    n->data = value;
    n->next = NULL;
    return n;
}

/* Insert a new node at the head */
void insert_head(Node **head, int value)
{
    Node *n = create_node(value);
    n->next = *head;
    *head = n;
}

/* Insert a new node at the tail */
void insert_tail(Node **head, int value)
{
    Node *n = create_node(value);

    if (*head == NULL) {
        *head = n;
        return;
    }

    Node *cur = *head;
    while (cur->next != NULL)
        cur = cur->next;
    cur->next = n;
}

/* Delete the first node with the given value.
 * Returns 1 if deleted, 0 if not found. */
int delete_value(Node **head, int value)
{
    Node *cur = *head;
    Node *prev = NULL;

    while (cur != NULL) {
        if (cur->data == value) {
            if (prev == NULL)
                *head = cur->next;   /* deleting head */
            else
                prev->next = cur->next;
            free(cur);
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}

/* Search for a value. Returns 1 if found, 0 otherwise. */
int search(Node *head, int value)
{
    for (Node *cur = head; cur != NULL; cur = cur->next)
        if (cur->data == value)
            return 1;
    return 0;
}

/* Print the list */
void print_list(Node *head)
{
    printf("[");
    for (Node *cur = head; cur != NULL; cur = cur->next) {
        printf("%d", cur->data);
        if (cur->next != NULL)
            printf(" -> ");
    }
    printf("]\n");
}

/* Free all nodes */
void free_list(Node **head)
{
    Node *cur = *head;
    while (cur != NULL) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    *head = NULL;
}

int main(void)
{
    Node *head = NULL;

    printf("=== Linked List Demo ===\n\n");

    /* Insert at head */
    insert_head(&head, 10);
    insert_head(&head, 20);
    insert_head(&head, 30);
    printf("After insert_head 10, 20, 30:\n");
    print_list(head);   /* [30 -> 20 -> 10] */

    /* Insert at tail */
    insert_tail(&head, 40);
    insert_tail(&head, 50);
    printf("\nAfter insert_tail 40, 50:\n");
    print_list(head);   /* [30 -> 20 -> 10 -> 40 -> 50] */

    /* Search */
    printf("\nSearch 10: %s\n", search(head, 10) ? "found" : "not found");
    printf("Search 99: %s\n", search(head, 99) ? "found" : "not found");

    /* Delete */
    printf("\nDeleting 20...\n");
    delete_value(&head, 20);
    print_list(head);   /* [30 -> 10 -> 40 -> 50] */

    printf("\nDeleting 30 (head)...\n");
    delete_value(&head, 30);
    print_list(head);   /* [10 -> 40 -> 50] */

    printf("\nDeleting 99 (not present)...\n");
    delete_value(&head, 99);
    print_list(head);   /* [10 -> 40 -> 50] */

    /* Clean up */
    free_list(&head);
    printf("\nAfter free_list:\n");
    print_list(head);   /* [] */

    return 0;
}

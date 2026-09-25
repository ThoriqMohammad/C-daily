/* queue_simulation.c
 *
 * A queue implementation using a circular buffer.
 *
 * A queue is a FIFO (First In, First Out) data structure.
 * Elements are added at the "rear" and removed from the "front".
 *
 * This implementation uses a circular buffer:
 *   - The queue is stored in a fixed-size array.
 *   - Two indices track the front and rear.
 *   - When an index reaches the end, it wraps to 0.
 *
 * Usage:
 *   ./queue_simulation
 *
 * Skills practiced:
 *   - Arrays and indices
 *   - Circular buffer logic
 *   - Modulo arithmetic
 *   - FIFO data structure
 *   - Queue operations: enqueue, dequeue, peek, empty, full
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define QUEUE_SIZE 10

typedef struct {
    int data[QUEUE_SIZE];
    int front;   /* index of the front element */
    int rear;    /* index where the next element will be inserted */
    int count;   /* number of elements currently in the queue */
} Queue;

/* Initialize the queue */
void queue_init(Queue *q)
{
    q->front = 0;
    q->rear = 0;
    q->count = 0;
}

/* Check if the queue is empty */
bool queue_is_empty(const Queue *q)
{
    return q->count == 0;
}

/* Check if the queue is full */
bool queue_is_full(const Queue *q)
{
    return q->count == QUEUE_SIZE;
}

/* Add an element to the rear of the queue */
bool enqueue(Queue *q, int value)
{
    if (queue_is_full(q))
        return false;

    q->data[q->rear] = value;
    q->rear = (q->rear + 1) % QUEUE_SIZE;
    q->count++;
    return true;
}

/* Remove and return the element at the front */
bool dequeue(Queue *q, int *value)
{
    if (queue_is_empty(q))
        return false;

    *value = q->data[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->count--;
    return true;
}

/* Look at the front element without removing it */
bool peek(const Queue *q, int *value)
{
    if (queue_is_empty(q))
        return false;

    *value = q->data[q->front];
    return true;
}

/* Print the queue contents */
void queue_print(const Queue *q)
{
    printf("Queue (front → rear): [");

    for (int i = 0; i < q->count; i++)
    {
        int index = (q->front + i) % QUEUE_SIZE;
        printf("%d", q->data[index]);
        if (i < q->count - 1)
            printf(", ");
    }

    printf("]\n");
}

int main(void)
{
    Queue q;
    int value;

    queue_init(&q);

    printf("====================================\n");
    printf("       QUEUE SIMULATION             \n");
    printf("====================================\n\n");

    /* Enqueue some values */
    printf("Enqueuing: 10, 20, 30, 40, 50\n");
    enqueue(&q, 10);
    enqueue(&q, 20);
    enqueue(&q, 30);
    enqueue(&q, 40);
    enqueue(&q, 50);
    queue_print(&q);

    /* Peek */
    if (peek(&q, &value))
        printf("\nFront element (peek): %d\n", value);

    /* Dequeue two elements */
    printf("\nDequeuing two elements...\n");
    dequeue(&q, &value);
    printf("Dequeued: %d\n", value);
    dequeue(&q, &value);
    printf("Dequeued: %d\n", value);
    queue_print(&q);

    /* Enqueue more (tests wrap-around) */
    printf("\nEnqueuing: 60, 70, 80, 90, 100, 110, 120\n");
    enqueue(&q, 60);
    enqueue(&q, 70);
    enqueue(&q, 80);
    enqueue(&q, 90);
    enqueue(&q, 100);
    enqueue(&q, 110);
    enqueue(&q, 120);
    queue_print(&q);

    /* Try to enqueue when full */
    if (!enqueue(&q, 999))
        printf("\nQueue is full—cannot enqueue 999.\n");

    /* Empty the queue */
    printf("\nEmptying the queue...\n");
    while (dequeue(&q, &value))
        printf("Dequeued: %d\n", value);

    printf("\nQueue is now empty.\n");
    queue_print(&q);

    /* Try to dequeue from an empty queue */
    if (!dequeue(&q, &value))
        printf("\nQueue is empty—cannot dequeue.\n");

    return 0;
}

/* ring_buffer.c
 *
 * A fixed-capacity ring buffer (circular queue) for integers.
 *
 * A ring buffer stores items in a fixed-size array and reuses
 * the space by wrapping around. Two indices track the ends:
 *
 *   head -> where the next item will be read from
 *   tail -> where the next item will be written to
 *
 * Both indices advance modulo the capacity, so the buffer
 * never has to shift elements. Push and pop are O(1).
 *
 * The tricky part is telling "full" apart from "empty":
 *
 *   empty : head == tail
 *   full  : (tail + 1) % cap == head
 *
 * We deliberately waste one slot so the two states are
 * distinguishable without a separate counter.
 *
 * Usage:
 *   ./ring_buffer
 *
 * Skills practiced:
 *   - struct-based data structure design
 *   - Modular arithmetic for circular indexing
 *   - Dynamic allocation with malloc and free
 *   - Distinguishing full vs empty without an extra flag
 *   - Writing self-tests in main()
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    int *data;      /* backing array                      */
    size_t cap;     /* usable capacity is cap - 1         */
    size_t head;    /* index of next item to read         */
    size_t tail;    /* index of next slot to write        */
} ring_buffer;

/* Allocate a ring buffer able to hold up to 'capacity' items */
ring_buffer *rb_create(size_t capacity)
{
    ring_buffer *rb = malloc(sizeof(*rb));
    if (rb == NULL)
        return NULL;

    /* +1 for the sentinel slot that distinguishes full from empty */
    rb->data = malloc((capacity + 1) * sizeof(*rb->data));
    if (rb->data == NULL)
    {
        free(rb);
        return NULL;
    }

    rb->cap  = capacity + 1;
    rb->head = 0;
    rb->tail = 0;
    return rb;
}

/* Free everything the ring buffer owns */
void rb_destroy(ring_buffer *rb)
{
    if (rb == NULL)
        return;
    free(rb->data);
    free(rb);
}

/* Is the buffer empty? */
int rb_empty(const ring_buffer *rb)
{
    return rb->head == rb->tail;
}

/* Is the buffer full? */
int rb_full(const ring_buffer *rb)
{
    return ((rb->tail + 1) % rb->cap) == rb->head;
}

/* Number of items currently stored */
size_t rb_size(const ring_buffer *rb)
{
    if (rb->tail >= rb->head)
        return rb->tail - rb->head;
    return rb->cap - rb->head + rb->tail;
}

/* Add an item to the back. Returns 0 on success, -1 if full. */
int rb_push(ring_buffer *rb, int value)
{
    if (rb_full(rb))
        return -1;

    rb->data[rb->tail] = value;
    rb->tail = (rb->tail + 1) % rb->cap;
    return 0;
}

/* Remove an item from the front. Returns 0 on success, -1 if empty. */
int rb_pop(ring_buffer *rb, int *out)
{
    if (rb_empty(rb))
        return -1;

    *out = rb->data[rb->head];
    rb->head = (rb->head + 1) % rb->cap;
    return 0;
}

/* Print the current contents in logical order, front to back */
void rb_print(const ring_buffer *rb)
{
    printf("[");
    size_t i = rb->head;
    while (i != rb->tail)
    {
        printf("%d", rb->data[i]);
        i = (i + 1) % rb->cap;
        if (i != rb->tail)
            printf(", ");
    }
    printf("]");
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-40s %s\n", label, condition ? "ok" : "FAIL");
}

static void test_basic(void)
{
    printf("Test 1: basic push/pop\n");

    ring_buffer *rb = rb_create(4);
    if (rb == NULL)
    {
        perror("rb_create");
        exit(EXIT_FAILURE);
    }

    check("new buffer is empty", rb_empty(rb));
    check("new buffer size is 0", rb_size(rb) == 0);

    rb_push(rb, 10);
    rb_push(rb, 20);
    rb_push(rb, 30);

    check("size after 3 pushes", rb_size(rb) == 3);
    check("not full at 3 of 4", !rb_full(rb));

    int v;
    rb_pop(rb, &v);
    check("first pop returns 10", v == 10);
    rb_pop(rb, &v);
    check("second pop returns 20", v == 20);

    rb_destroy(rb);
    printf("\n");
}

static void test_wrap(void)
{
    printf("Test 2: wraparound behavior\n");

    ring_buffer *rb = rb_create(3);
    if (rb == NULL)
    {
        perror("rb_create");
        exit(EXIT_FAILURE);
    }

    /* Fill and drain once to move head and tail off zero */
    rb_push(rb, 1);
    rb_push(rb, 2);
    rb_push(rb, 3);
    int v;
    rb_pop(rb, &v);
    rb_pop(rb, &v);

    /* Now head is at 2, tail is at 0 after wrap */
    rb_push(rb, 4);
    rb_push(rb, 5);

    printf("  contents after wrap: ");
    rb_print(rb);
    printf("\n");

    check("size is 3", rb_size(rb) == 3);
    check("buffer reports full", rb_full(rb));

    rb_pop(rb, &v);
    check("front is 3", v == 3);
    rb_pop(rb, &v);
    check("next is 4", v == 4);
    rb_pop(rb, &v);
    check("last is 5", v == 5);
    check("buffer empty again", rb_empty(rb));

    rb_destroy(rb);
    printf("\n");
}

static void test_full_and_empty(void)
{
    printf("Test 3: full and empty edge cases\n");

    ring_buffer *rb = rb_create(2);
    if (rb == NULL)
    {
        perror("rb_create");
        exit(EXIT_FAILURE);
    }

    int v;
    check("pop on empty fails", rb_pop(rb, &v) == -1);

    rb_push(rb, 7);
    rb_push(rb, 8);
    check("push to full fails", rb_push(rb, 9) == -1);

    rb_destroy(rb);
    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("           RING BUFFER              \n");
    printf("====================================\n\n");

    test_basic();
    test_wrap();
    test_full_and_empty();

    printf("All tests complete.\n");
    return 0;
}

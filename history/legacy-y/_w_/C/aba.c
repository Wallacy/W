
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

typedef struct _Node
{
    int data;
    struct _Node *next;
} Node;

typedef struct _lfstack_t
{
    int tag;
    Node *head;
} lfstack_t;

void lfstack_push(_Atomic lfstack_t *lfstack, int value)
{
    lfstack_t next;
    lfstack_t orig = atomic_load(lfstack);
    Node *node = malloc(sizeof(Node));
    node->data = value;
    do
    {
        node->next = orig.head;
        next.head = node;
        next.tag = orig.tag + 1;

    } while (!atomic_compare_exchange_weak(lfstack, &orig, next));
}

int lfstack_pop(_Atomic lfstack_t *lfstack)
{
    lfstack_t next;
    lfstack_t orig = atomic_load(lfstack);
    do
    {
        if (orig.head == NULL)
        {
            return -1;
        }

        next.head = orig.head->next;
        next.tag = orig.tag + 1;

    } while (!atomic_compare_exchange_weak(lfstack, &orig, next));

    printf("poping value %d\n", orig.head->data);

    free(orig.head);

    return 0;
}

// Initialize the atomic stack
lfstack_t initial_stack = {.tag = 0, .head = NULL};
_Atomic lfstack_t top;

void initialize_stack()
{
    atomic_init(&top, initial_stack);
}

void *push(void *input)
{
    for (int i = 0; i < 100000; i++)
    {
        lfstack_push(&top, i);
        printf("push %d\n", i);
    }
    pthread_exit(NULL);
}

void *pop(void *input)
{
    for (int i = 0; i < 100000;)
    {
        int result = lfstack_pop(&top);
        if (result == 0)
        { // Successfully popped an item
            i++;
        }
        // Optionally add a small sleep or yield if pop fails frequently
        // to avoid busy-waiting when the stack is empty.
        // else { usleep(10); } // Example using unistd.h
    }
    pthread_exit(NULL);
}

int main()
{
    initialize_stack(); // Initialize the global atomic stack

    pthread_t tid[200];
    for (int i = 0; i < 100; i++)
        pthread_create(&tid[i], NULL, push, NULL);
    for (int i = 0; i < 200; i++)
        pthread_join(tid[i], NULL);
    return 0;
}
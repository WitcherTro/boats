#ifndef GENERIC_QUEUE_H
#define GENERIC_QUEUE_H

#include <pthread.h>

typedef struct QueueNode {
    void *data;
    struct QueueNode *next;
} QueueNode;

typedef struct GenericQueue {
    QueueNode *head;
    QueueNode *tail;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} GenericQueue;

/* Initialize the queue */
void queue_init(GenericQueue *q);

/* Add an item to the queue (thread-safe) */
void queue_push(GenericQueue *q, void *data);

/* Remove and return an item from the queue (blocks if empty) */
void *queue_pop(GenericQueue *q);

/* Destroy the queue and free nodes (does not free data) */
void queue_destroy(GenericQueue *q);

#endif

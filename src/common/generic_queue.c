#include "generic_queue.h"
#include <stdlib.h>

void queue_init(GenericQueue *q) {
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
}

void queue_push(GenericQueue *q, void *data) {
    QueueNode *node = (QueueNode *)malloc(sizeof(QueueNode));
    if (!node) return;
    
    node->data = data;
    node->next = NULL;
    
    pthread_mutex_lock(&q->lock);
    
    if (q->tail) {
        q->tail->next = node;
        q->tail = node;
    } else {
        q->head = node;
        q->tail = node;
    }
    q->size++;
    
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

void *queue_pop(GenericQueue *q) {
    pthread_mutex_lock(&q->lock);
    
    while (q->size == 0) {
        pthread_cond_wait(&q->cond, &q->lock);
    }
    
    QueueNode *node = q->head;
    void *data = node->data;
    
    q->head = node->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    q->size--;
    
    pthread_mutex_unlock(&q->lock);
    
    free(node);
    return data;
}

void queue_destroy(GenericQueue *q) {
    pthread_mutex_lock(&q->lock);
    QueueNode *current = q->head;
    while (current != NULL) {
        QueueNode *next = current->next;
        free(current);
        current = next;
    }
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    pthread_mutex_unlock(&q->lock);
    
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
}

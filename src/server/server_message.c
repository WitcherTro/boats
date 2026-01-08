#include "server_message.h"
#include "generic_queue.h"
#include <stdlib.h>

static GenericQueue msg_queue;

void message_queue_init(void) {
    queue_init(&msg_queue);
}

void enqueue_msg(char *msg, int sender) {
    MsgEntry *entry = (MsgEntry *)malloc(sizeof(MsgEntry));
    if (entry) {
        entry->msg = msg;
        entry->sender = sender;
        queue_push(&msg_queue, entry);
    }
}

MsgEntry dequeue_msg(void) {
    MsgEntry *ptr = (MsgEntry *)queue_pop(&msg_queue);
    MsgEntry e = *ptr;
    free(ptr); /* Free the container, but not the message content (caller handles that) */
    return e;
}

void message_queue_cleanup(void) {
    /* Iterate manually to free data before destroying nodes */
    pthread_mutex_lock(&msg_queue.lock);
    QueueNode *current = msg_queue.head;
    while (current != NULL) {
        MsgEntry *entry = (MsgEntry *)current->data;
        if (entry) {
            if (entry->msg) free(entry->msg);
            free(entry);
        }
        current = current->next;
    }
    pthread_mutex_unlock(&msg_queue.lock);
    
    queue_destroy(&msg_queue);
}

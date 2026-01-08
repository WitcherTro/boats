#ifndef SERVER_MESSAGE_H
#define SERVER_MESSAGE_H

#include <pthread.h>

/* Message queue entry */
typedef struct MsgEntry {
    char *msg;
    int sender;
} MsgEntry;

/* Initialize message queue */
void message_queue_init(void);

/* Enqueue a message from a client */
void enqueue_msg(char *msg, int sender);

/* Dequeue and return next message (blocks if queue empty) */
MsgEntry dequeue_msg(void);

/* Cleanup message queue and free remaining messages */
void message_queue_cleanup(void);

#endif /* SERVER_MESSAGE_H */

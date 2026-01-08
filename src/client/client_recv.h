#ifndef CLIENT_RECV_H
#define CLIENT_RECV_H

/*
 * client_recv.h - Client receiver thread and server message handling
 */

/* Thread function: reads server messages and updates game state */
void *recv_thread(void *arg);

#endif /* CLIENT_RECV_H */

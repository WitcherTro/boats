#ifndef SERVER_CLIENT_H
#define SERVER_CLIENT_H

#include "server_state.h"
#include <pthread.h>

/* Client reader thread - reads from client socket and enqueues messages */
void *client_reader(void *arg);

#endif /* SERVER_CLIENT_H */

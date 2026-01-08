#ifndef SERVER_COMMANDS_H
#define SERVER_COMMANDS_H

#include "server_state.h"

/* Handle NAME command from client */
void handle_name_command(ServerState *state, const char *msg, int sender);

/* Handle PLACE command from client */
void handle_place_command(ServerState *state, const char *msg, int sender);

/* Handle MOVE command from client */
void handle_move_command(ServerState *state, const char *msg, int sender);

/* Handle READY command from client */
void handle_ready_command(ServerState *state, int sender);

/* Handle FIRE command from client */
void handle_fire_command(ServerState *state, const char *msg, int sender);

/* Handle DISCONNECT event */
void handle_disconnect(ServerState *state, int sender, sock_t *listen_fd_ptr);

/* Handle rematch response */
void handle_rematch_response(ServerState *state, int sender, int response);

#endif /* SERVER_COMMANDS_H */

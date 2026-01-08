#ifndef CLIENT_COMMANDS_H
#define CLIENT_COMMANDS_H

/*
 * client_commands.h - Client command parsing and execution (FIRE, PLACE, RANDOM)
 */

/* Handle FIRE command: parse coordinates and send to server */
void handle_fire_command(const char *args);

/* Handle PLACE command: parse coordinates and send to server */
void handle_place_command(const char *args);

/* Handle RANDOM command: auto-generate and place all remaining ships */
void handle_random_command(void);

/* Handle MOVE command: move a ship to a new location */
void handle_move_command(const char *args);

/* Handle READY command: signal ready to start game */
void handle_ready_command(void);

/* Handle HELP command: show available commands */
void handle_help_command(void);

#endif /* CLIENT_COMMANDS_H */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT 12345
#define MAX_LINE 512

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define SOCKET_INVALID INVALID_SOCKET
#define READ(s, b, n) recv((s), (b), (int)(n), 0)
#define WRITE(s, b, n) send((s), (b), (int)(n), 0)
#define CLOSE(s) closesocket(s)
int sock_init(void);
void sock_cleanup(void);
#define SHUT_RDWR_FLAG SD_BOTH
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int sock_t;
#define SOCKET_INVALID (-1)
#define READ(s, b, n) read((s), (b), (n))
#define WRITE(s, b, n) write((s), (b), (n))
#define CLOSE(s) close(s)
static inline int sock_init(void) { return 0; }
static inline void sock_cleanup(void) { }
#define SHUT_RDWR_FLAG SHUT_RDWR
#endif

/* Read a line (newline-terminated) from socket into buf */
/* ssize_t is not standard on Windows MSVC, usually needed to be defined or use int/long long */
#ifdef _WIN32
  #include <BaseTsd.h>
  typedef SSIZE_T ssize_t;
#endif

ssize_t read_line(sock_t fd, char *buf, size_t maxlen);

/* Write UTF-8 string to console (handles Windows console API) */
void print_utf8(const char *str);

#endif /* COMMON_H */

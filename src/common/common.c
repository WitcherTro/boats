#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include <errno.h>
#else
#include <errno.h>
#endif

ssize_t read_line(sock_t fd, char *buf, size_t maxlen) {
    char *ptr = buf;
    char *end = buf + maxlen - 1;
    
    while (ptr < end) {
        char c;
        ssize_t n = READ(fd, &c, 1);
        if (n > 0) {
            *ptr++ = c;
            if (c == '\n') break;
        } else if (n == 0) {
            if (ptr == buf) return 0;
            break;
        } else {
            if (errno == EINTR) continue;
            return -1;
        }
    }
    *ptr = '\0';
    return ptr - buf;
}

#ifdef _WIN32
int sock_init(void) {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
}

void sock_cleanup(void) {
    WSACleanup();
}
#endif

/* Write UTF-8 string directly to console on Windows to bypass printf encoding issues */
void print_utf8(const char *str) {
#ifdef _WIN32
    if (!str) return;
    
    /* Try Windows console API for UTF-8 output */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
        if (wlen > 1) {
            wchar_t *wbuf = malloc(wlen * sizeof(wchar_t));
            if (wbuf) {
                MultiByteToWideChar(CP_UTF8, 0, str, -1, wbuf, wlen);
                WriteConsoleW(hOut, wbuf, wlen - 1, NULL, NULL);
                free(wbuf);
                return;
            }
        }
    }
#endif
    /* Fallback to printf on all platforms */
    printf("%s", str);
}

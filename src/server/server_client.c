#include "server_client.h"
#include "server_message.h"
#include "common.h"
#include <stdlib.h>
#include <string.h>

void *client_reader(void *arg) {
    ClientCtx *ctx = arg;
    char buf[4096];
    size_t buf_len = 0;
    ssize_t n;
    
    while ((n = READ(ctx->fd, buf + buf_len, sizeof(buf) - 1 - buf_len)) > 0) {
        buf_len += n;
        buf[buf_len] = '\0';
        
        char *start = buf;
        char *newline;
        
        while ((newline = strchr(start, '\n')) != NULL) {
            *newline = '\0';
            
            /* Handle optional \r before \n */
            if (newline > start && *(newline - 1) == '\r') {
                *(newline - 1) = '\0';
            }
            
            /* Only enqueue non-empty lines */
            if (strlen(start) > 0) {
                char *m = strdup(start);
                if (m) enqueue_msg(m, ctx->connection_id);
            }
            
            start = newline + 1;
        }
        
        /* Move remaining data to front */
        size_t remaining = buf + buf_len - start;
        if (remaining > 0 && start > buf) {
            memmove(buf, start, remaining);
        }
        buf_len = remaining;
        
        /* Buffer full protection */
        if (buf_len >= sizeof(buf) - 1) {
            buf[buf_len] = '\0';
            char *m = strdup(buf);
            if (m) enqueue_msg(m, ctx->connection_id);
            buf_len = 0;
        }
    }
    
    /* Client disconnected or read error - inform main loop */
    char *disc = strdup("DISCONNECT\n");
    if (disc) enqueue_msg(disc, ctx->connection_id);
    /* Do NOT close fd here, let main thread handle it via handle_disconnect */
    /* CLOSE(ctx->fd); */
    
    /* ctx is managed by global state main loop, do not free here if it is still in the array */
    /* Because connection_id logic mapping relies on it. 
       Actually, connection is done. 
       But allowing main thread to cleanup is safer. */
    // free(ctx); 
    
    /* Update active thread count */
    if (g_global_state) {
        pthread_mutex_lock(&g_global_state->lock);
        g_global_state->active_threads--;
        pthread_mutex_unlock(&g_global_state->lock);
    }
    
    return NULL;
}


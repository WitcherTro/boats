#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int client_run(const char *host, int port);

#ifdef BUILD_GUI
int gui_main(const char *host, int port);
#endif

int main(int argc, char **argv) {
    int use_cli = 0;
    char host[256] = "127.0.0.1";
    int port = 12345;
    int args_provided = 0;
    
    /* Check for -cli flag and other args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-cli") == 0) {
            use_cli = 1;
        } else if (!args_provided) {
            /* Assume first non-flag arg is host */
            strncpy(host, argv[i], sizeof(host)-1);
            if (i + 1 < argc) {
                port = atoi(argv[i+1]);
            }
            args_provided = 1;
        }
    }
    
#ifdef BUILD_GUI
    if (!use_cli) {
        /* Launch GUI mode */
        /* If args provided, pass them. If not, pass NULL to trigger GUI input */
        if (args_provided) {
            return gui_main(host, port);
        } else {
            return gui_main(NULL, 0);
        }
    }
#endif

    if (!args_provided) {
        printf("Enter server IP (default 127.0.0.1): ");
        char input[256];
        if (fgets(input, sizeof(input), stdin)) {
            size_t len = strlen(input);
            if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';
            if (strlen(input) > 0) {
                strncpy(host, input, sizeof(host)-1);
            }
        }
        
        printf("Enter server port (default 12345): ");
        if (fgets(input, sizeof(input), stdin)) {
            size_t len = strlen(input);
            if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';
            if (strlen(input) > 0) {
                port = atoi(input);
            }
        }
    }
    
    /* Launch CLI mode */
    return client_run(host, port);
}

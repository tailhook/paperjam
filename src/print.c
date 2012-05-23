#include <stdio.h>

void print_message(char *msg, size_t len, int more) {
    if(more) {
        printf("\"%.*s\" ", (int)len, msg);
    } else {
        printf("\"%.*s\"\n", (int)len, msg);
        fflush(stdout);
    }
}

#include <stdio.h>
#include <ctype.h>

void print_message(char *data, size_t len, int more) {
    FILE *stream = stdout;
    fputc('"', stream);
    for(char *c = data, *end = data + len; c < end; ++c) {
        if(isprint(*c)) {
            if(*c == '\\') {
                fprintf(stream, "\\\\");
            } else if(*c == '"') {
                fprintf(stream, "\\\"");
            } else {
                fputc(*c, stream);
            }
        } else if(*c == '\r') {
            fprintf(stream, "\\r");
        } else if(*c == '\n') {
            fprintf(stream, "\\n");
        } else {
            fprintf(stream, "-\\x%02hhx", *c);
        }
    }
    fputc('"', stream);
    if(more)
        fputc(' ', stream);
    else
        fputc('\n', stream);
    fflush(stream);
}

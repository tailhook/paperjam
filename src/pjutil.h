#ifndef _H_PJUTIL
#define _H_PJUTIL

#include <getopt.h>


struct cli_options_s {
    int defaultlib;
    int arg0_lib;
    int arg0_sock;
    int lib;
    int sock;
    double timeout;
    double finishtime;
    int argc;
    char **argv;
    char **messages;
};


extern struct cli_options_s cli_options;
extern char *short_options;
extern struct option long_options[];

void print_message(char *data, size_t datalen, int more);
double get_time();

#endif //_H_PJUTIL

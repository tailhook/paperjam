#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <config.h>
#include <time.h>
#include <assert.h>
#include "pjutil.h"
#ifdef HAVE_XS
#include <xs.h>
#include "xs_cli.h"
#endif
#ifdef HAVE_ZMQ
#include <zmq.h>
#include "zmq_cli.h"
#endif


struct cli_options_s cli_options;


struct {
    char pattern[2];
    void (*function)();
} table[] = {
# if HAVE_XS
    {"Xp", run_xs_pull},
    {"XP", run_xs_push},
    {"Xs", run_xs_sub},
    {"XS", run_xs_pub},
    {"XR", run_xs_req},
    {"Xr", run_xs_rep},
# ifdef HAVE_SURVEY
    {"XU", run_xs_surveyor},
    {"Xu", run_xs_respondent},
# endif
# endif
# if HAVE_ZMQ
    {"Zp", run_zmq_pull},
    {"ZP", run_zmq_push},
    {"Zs", run_zmq_sub},
    {"ZS", run_zmq_pub},
    {"ZR", run_zmq_req},
    {"Zr", run_zmq_rep},
# endif
    {"\0\0", NULL}
};

char *short_options = "hZXpPsSrRb:c:t:";
struct option long_options[] = {
# ifdef HAVE_ZMQ
    {"zmq", 0, NULL, 'Z'},
# endif
# ifdef HAVE_XS
    {"xs", 0, NULL, 'X'},
# endif
    {"pull", 0, NULL, 'p'},
    {"push", 0, NULL, 'P'},
    {"sub", 0, NULL, 's'},
    {"pub", 0, NULL, 'S'},
    {"req", 0, NULL, 'R'},
    {"rep", 0, NULL, 'r'},
# ifdef HAVE_SURVEY
    {"surveyor", 0, NULL, 'U'},
    {"respondent", 0, NULL, 'u'},
# endif
    {"bind", 1, NULL, 'b'},
    {"connect", 1, NULL, 'c'},
    {"timeout", 1, NULL, 't'},
    {"help", 0, NULL, 'h'},
    {NULL, 0, NULL, 0}
    };


void print_usage(FILE *stream, char *arg0) {
    fprintf(stream, "Usage:\n");
    char *stype = "{--push|--pull"
        "|--pub|--sub"
        "|--req|--rep"
# if HAVE_SURVEY
        "|--surveyor|--respondent"
# endif
        "}";
    if(cli_options.arg0_lib && cli_options.arg0_sock) {
        fprintf(stream, "  %s {--bind|--connect} ADDR [msg1 [msg2 ...]]\n",
            arg0);
    } else if(cli_options.arg0_lib) {
        if(cli_options.arg0_lib == 'X') {
            // zmq has no surveyor
            stype = "{--push|--pull|--pub|--sub|--req|--rep}";
        }
        fprintf(stream, "  %s %s\\\n"
        "    {--bind|--connect} ADDR [msg1 [msg2 ...]]\n", arg0, stype);
    } else if(cli_options.arg0_sock) {
        fprintf(stream, "  %s [--zmq|--xs] "
                        "{--bind|--connect} ADDR [msg1 [msg2 ...]]\n", arg0);
    } else {
# if defined(HAVE_ZMQ) && defined(HAVE_XS)
        fprintf(stream,
            "  %s [--zmq|--xs] %s \\\n"
            "    {--bind|--connect} ADDR [options] [msg1 [msg2 ...]]\n",
            arg0, stype);
# else
        fprintf(stream,
            "  %s %s\\\n"
            "    {--bind|--connect} ADDR [options] [msg1 [msg2 ...]]\n",
            arg0, stype);
# endif
    }
    fprintf(stream, "\n"
        "Options:\n");
# ifdef HAVE_ZMQ
    if(cli_options.defaultlib == 'Z') {
        fprintf(stream, " -Z,--zmq   Open zeromq socket (default)\n");
    } else {
        fprintf(stream, " -Z,--zmq   Open zeromq socket\n");
    }
# endif
# ifdef HAVE_XS
    if(cli_options.defaultlib == 'X') {
        fprintf(stream,
            " -X,--xs    Open crossroads IO (libxs) socket (default)\n");
    } else {
        fprintf(stream, " -X,--xs   Open crossroads IO (libxs) socket\n");
    }
# endif
    fprintf(stream,
        " -p,--pull       Open PULL socket and print all incoming data\n"
        " -P,--push       Open PUSH socket and push messages\n"
        " -S,--pub        Open PUB socket and publish messages\n"
        " -s,--sub        Open SUB socket and print all incoming data\n"
        " -R,--req        Open REQ socket and send a request\n"
        " -r,--rep        Open REP socket and answer requests with same data\n"
        " -U,--surveyor   Open SURVEYOR socket and send a request"
                          " (if supported)\n"
        " -u,--respondent Open RESPONDENT socket and answer requests"
                          " (if supported)\n"
        " -b,--bind ADDR  Bind socket to address. ADDR is zeromq/libxs address\n"
        " -c,--connect ADDR\n"
        "                 Connect socket to address. ADDR is zeromq/libxs address\n"
        " -t,--timeout SEC\n"
        "                 Maximum time process runs, in seconds\n"
        "\n"
        );
}

double get_time() {
    struct timespec ts;
    int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(rc != -1);
    return (double)ts.tv_sec  + ts.tv_nsec*0.000000001;
}

void clear_options(int argc, char **argv) {
# if HAVE_XS
    cli_options.defaultlib = 'X';
# else
    cli_options.defaultlib = 'Z';
# endif
    cli_options.arg0_lib = 0;
    cli_options.arg0_sock = 0;
    cli_options.lib = 0;
    cli_options.sock = 0;
    cli_options.messages = NULL;
    cli_options.argc = argc;
    cli_options.argv = argv;
    cli_options.timeout = 0.0;
    cli_options.finishtime = 1e20;
}

void parse_arg0(char *arg0) {
    if(strstr(arg0, "xs")) {
        cli_options.arg0_lib = 'X';
    } else if(strstr(arg0, "zmq")) {
        cli_options.arg0_lib = 'Z';
    }
    if(strstr(arg0, "push")) {
        cli_options.arg0_sock = 'P';
    } else if(strstr(arg0, "pull")) {
        cli_options.arg0_sock = 'p';
    } else if(strstr(arg0, "pub")) {
        cli_options.arg0_sock = 'S';
    } else if(strstr(arg0, "sub")) {
        cli_options.arg0_sock = 's';
    } else if(strstr(arg0, "req")) {
        cli_options.arg0_sock = 'R';
    } else if(strstr(arg0, "rep")) {
        cli_options.arg0_sock = 'r';
    } else if(strstr(arg0, "surveyor")) {
        cli_options.arg0_sock = 'U';
    } else if(strstr(arg0, "respondent")) {
        cli_options.arg0_sock = 'u';
    }
}

void parse_options(int argc, char **argv) {
    int opt;
    while((opt = getopt_long(argc, argv,
                             short_options, long_options, NULL)) != -1) {
        switch(opt) {
        case 'Z':
            cli_options.lib = 'Z';
            break;
        case 'X':
            cli_options.lib = 'X';
            break;
        case 't':
            cli_options.timeout = strtod(optarg, NULL);
            cli_options.finishtime = get_time() + cli_options.timeout;
            break;
        case 'p': case 'P':
        case 's': case 'S':
        case 'r': case 'R':
        case 'u': case 'U':
            cli_options.sock = opt;
            break;
        case 'h':
            print_usage(stdout, argv[0]);
            exit(0);
        case '?':
            print_usage(stderr, argv[0]);
            exit(1);
        }
    }
    cli_options.messages = argv + optind;
}

int main(int argc, char **argv) {

    clear_options(argc, argv);
    char *arg0 = strrchr(argv[0], '/');
    if(arg0) arg0 += 1;
    else arg0 = argv[0];
    parse_arg0(argv[0]);
    parse_options(argc, argv);

    int sock = cli_options.sock;
    if(!sock) sock = cli_options.arg0_sock;
    if(!cli_options.sock && !cli_options.arg0_sock) {
        fprintf(stderr, "Socket type is not specified\n");
        print_usage(stderr, argv[0]);
        exit(1);
    }

    int lib = cli_options.lib;
    if(!lib) lib = cli_options.arg0_lib;
    if(!lib) lib = cli_options.defaultlib;
    if(!lib) {
        fprintf(stderr, "Library used (libxs or libzmq) is not specified\n");
        exit(0);
    }

    for(int i = 0; i < sizeof(table)/sizeof(table[0]); ++i) {
        if(table[i].pattern[0] == lib && table[i].pattern[1] == sock) {
            table[i].function();
            exit(0);
        }
    }
    fprintf(stderr, "Wrong combination of library and socket type\n");
    exit(1);
}

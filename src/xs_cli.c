#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <xs.h>

#include "config.h"
#include "pjutil.h"

static void *open_context() {
    return xs_init();
}

static void configure_socket(void *sock) {
    int opt;
    int rc;
    optind = 1;
    while((opt = getopt_long(cli_options.argc, cli_options.argv,
                             short_options, long_options, NULL)) != -1) {
        switch(opt) {
        case 'b':
            rc = xs_bind(sock, optarg);
            assert(rc != -1);
            break;
        case 'c':
            rc = xs_connect(sock, optarg);
            assert(rc != -1);
            break;
        }
    }
}

static int do_receive(void *sock, xs_msg_t *msg) {
    xs_pollitem_t item = {sock, 0, XS_POLLIN, 0 };
    while(get_time() < cli_options.finishtime) {
        int left = -1;
        if(cli_options.timeout) {
            left = (int)((cli_options.finishtime - get_time())*1000);
        }
        int rc = xs_poll(&item, 1, left);
        rc = xs_recvmsg(sock, msg, XS_DONTWAIT);
        if(rc == -1) {
            if(errno == EAGAIN) continue;
            else if(errno == EFSM) {
                return 1;
            } else {
                perror("Error reading from socket");
                exit(127);
            }
        }
        return 0;
    }
    return 1;
}

static int do_send(void *sock, char *data, size_t len, int more) {
    xs_pollitem_t item = {sock, 0, XS_POLLOUT, 0 };
    while(get_time() < cli_options.finishtime) {
        int left = -1;
        if(cli_options.timeout) {
            left = (int)((cli_options.finishtime - get_time())*1000);
        }
        int rc = xs_poll(&item, 1, left);
        rc = xs_send(sock, data, len, (more ? XS_SNDMORE : 0)|XS_DONTWAIT);
        if(rc == -1) {
            if(errno == EAGAIN) continue;
            else {
                perror("Error writing to socket");
                exit(127);
            }
        }
        return 0;
    }
    return 1;
}

static void do_finish(void *sock, void *ctx) {
    int linger = -1;
    if(cli_options.timeout) {
        linger = (int)((cli_options.finishtime - get_time())*1000);
    }
    xs_setsockopt(sock, XS_LINGER, &linger, sizeof(linger));
    xs_close(sock);
    xs_term(ctx);
}

static void reader_socket(int type) {
    int rc;
    void *ctx = open_context();
    assert(ctx);
    void *sock = xs_socket(ctx, type);
    assert(sock);
    configure_socket(sock);
    if(type == XS_SUB) {
        rc = xs_setsockopt(sock, XS_SUBSCRIBE, "", 0);
        assert(rc != -1);
    }
    while(1) {
        xs_msg_t msg;
        xs_msg_init(&msg);
        if(do_receive(sock, &msg)) break;
        uint64_t more = 0;
        size_t moresz = sizeof(more);
        rc = xs_getsockopt(sock, XS_RCVMORE, &more, &moresz);
        assert(rc != -1);
        print_message(xs_msg_data(&msg), xs_msg_size(&msg), more);
        xs_msg_close(&msg);
    }
    do_finish(sock, ctx);
}


static void writer_socket(int type) {
    void *ctx = open_context();
    assert(ctx);
    void *sock = xs_socket(ctx, type);
    assert(sock);
    configure_socket(sock);
    if(type == XS_PUB) {
        // Let's wait for subscriptions to be forwarded
        // TODO(pc) make this interval customizable
        xs_poll(NULL, 0, 100);
    }
    for(char **m = cli_options.messages; *m; ++m) {
        if(do_send(sock, *m, strlen(*m), *(m+1) ? XS_SNDMORE : 0)) break;
    }
    do_finish(sock, ctx);
}

void run_xs_pull() {
    reader_socket(XS_PULL);
}

void run_xs_push() {
    writer_socket(XS_PUSH);
}

void run_xs_sub() {
    reader_socket(XS_SUB);
}

void run_xs_pub() {
    writer_socket(XS_PUB);
}

static void request(int type) {
    int rc;
    void *ctx = open_context();
    assert(ctx);
    void *sock = xs_socket(ctx, type);
    assert(sock);
    configure_socket(sock);
    for(char **m = cli_options.messages; *m; ++m) {
        if(do_send(sock, *m, strlen(*m), *(m+1) ? XS_SNDMORE : 0)) break;
    }
    uint64_t more = 1;
    size_t moresz = sizeof(more);
    while(1) {
        xs_msg_t msg;
        xs_msg_init(&msg);
        if(do_receive(sock, &msg)) break;
        rc = xs_getsockopt(sock, XS_RCVMORE, &more, &moresz);
        assert(rc != -1);
        print_message(xs_msg_data(&msg), xs_msg_size(&msg), more);
        xs_msg_close(&msg);
    }
    do_finish(sock, ctx);
}

static void reply(int type) {
    int rc;
    void *ctx = open_context();
    assert(ctx);
    void *sock = xs_socket(ctx, type);
    assert(sock);
    configure_socket(sock);
    while(1) {
        uint64_t more = 1;
        size_t moresz = sizeof(more);
        while(more) {
            xs_msg_t msg;
            xs_msg_init(&msg);
            if(do_receive(sock, &msg)) goto end;
            rc = xs_getsockopt(sock, XS_RCVMORE, &more, &moresz);
            assert(rc != -1);
            print_message(xs_msg_data(&msg), xs_msg_size(&msg), more);
            xs_msg_close(&msg);
        }

        for(char **m = cli_options.messages; *m; ++m) {
            if(do_send(sock, *m, strlen(*m), *(m+1))) goto end;
        }
    }
end:
    do_finish(sock, ctx);
}


void run_xs_req() {
    request(XS_REQ);
}

void run_xs_rep() {
    reply(XS_REP);
}

# ifdef HAVE_SURVEY
void run_xs_surveyor() {
    request(XS_SURVEYOR);
}

void run_xs_respondent() {
    reply(XS_RESPONDENT);
}

# endif



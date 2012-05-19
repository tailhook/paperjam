#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <xs.h>

#include "pjutil.h"

static void *open_context() {
    return xs_init(1);
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

static void reader_socket(int type) {
    void *ctx = open_context();
    assert(ctx);
    void *sock = xs_socket(ctx, type);
    assert(sock);
    configure_socket(sock);
    if(type == XS_SUB) {
        int rc = xs_setsockopt(sock, XS_SUBSCRIBE, "", 0);
        assert(rc != -1);
    }
    while(1) {
        xs_msg_t msg;
        xs_msg_init(&msg);
        int rc = xs_recvmsg(sock, &msg, 0);
        assert(rc != -1);
        uint64_t more = 0;
        size_t moresz = sizeof(more);
        rc = xs_getsockopt(sock, XS_RCVMORE, &more, &moresz);
        assert(rc != -1);
        print_message(xs_msg_data(&msg), xs_msg_size(&msg), more);
        xs_msg_close(&msg);
    }
    xs_close(sock);
    xs_term(ctx);
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
        int rc = xs_send(sock, *m, strlen(*m), *(m+1) ? XS_SNDMORE : 0);
        assert(rc != -1);
    }
    xs_close(sock);
    xs_term(ctx);
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

void run_xs_req() {
    void *ctx = open_context();
    assert(ctx);
    void *sock = xs_socket(ctx, XS_REQ);
    assert(sock);
    configure_socket(sock);
    for(char **m = cli_options.messages; *m; ++m) {
        int rc = xs_send(sock, *m, strlen(*m), *(m+1) ? XS_SNDMORE : 0);
        assert(rc != -1);
    }
    uint64_t more = 1;
    size_t moresz = sizeof(more);
    while(more) {
        xs_msg_t msg;
        xs_msg_init(&msg);
        int rc = xs_recvmsg(sock, &msg, 0);
        assert(rc != -1);
        rc = xs_getsockopt(sock, XS_RCVMORE, &more, &moresz);
        assert(rc != -1);
        print_message(xs_msg_data(&msg), xs_msg_size(&msg), more);
        xs_msg_close(&msg);
    }
    xs_close(sock);
    xs_term(ctx);
}

void run_xs_rep() {
    void *ctx = open_context();
    assert(ctx);
    void *sock = xs_socket(ctx, XS_REP);
    assert(sock);
    configure_socket(sock);
    while(1) {
        uint64_t more = 1;
        size_t moresz = sizeof(more);
        while(more) {
            xs_msg_t msg;
            xs_msg_init(&msg);
            int rc = xs_recvmsg(sock, &msg, 0);
            assert(rc != -1);
            rc = xs_getsockopt(sock, XS_RCVMORE, &more, &moresz);
            assert(rc != -1);
            print_message(xs_msg_data(&msg), xs_msg_size(&msg), more);
            xs_msg_close(&msg);
        }

        for(char **m = cli_options.messages; *m; ++m) {
            int rc = xs_send(sock, *m, strlen(*m), *(m+1) ? XS_SNDMORE : 0);
            assert(rc != -1);
        }
    }
    xs_close(sock);
    xs_term(ctx);
}


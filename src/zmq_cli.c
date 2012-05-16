#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <zmq.h>

#include "pjutil.h"

static void *open_context() {
    return zmq_init(1);
}

static void configure_socket(void *sock) {
    int opt;
    int rc;
    optind = 1;
    while((opt = getopt_long(cli_options.argc, cli_options.argv,
                             short_options, long_options, NULL)) != -1) {
        switch(opt) {
        case 'b':
            rc = zmq_bind(sock, optarg);
            assert(rc != -1);
            break;
        case 'c':
            rc = zmq_connect(sock, optarg);
            assert(rc != -1);
            break;
        }
    }
}

static void reader_socket(int type) {
    void *ctx = open_context();
    assert(ctx);
    void *sock = zmq_socket(ctx, type);
    assert(sock);
    configure_socket(sock);
    if(type == ZMQ_SUB) {
        int rc = zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
        assert(rc != -1);
    }
    while(1) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        int rc = zmq_recv(sock, &msg, 0);
        assert(rc != -1);
        uint64_t more = 0;
        size_t moresz = sizeof(more);
        rc = zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &moresz);
        assert(rc != -1);
        print_message(zmq_msg_data(&msg), zmq_msg_size(&msg), more);
        zmq_msg_close(&msg);
    }
    zmq_close(sock);
    zmq_term(ctx);
}


static void writer_socket(int type) {
    void *ctx = open_context();
    assert(ctx);
    void *sock = zmq_socket(ctx, type);
    assert(sock);
    configure_socket(sock);
    for(char **m = cli_options.messages; *m; ++m) {
        zmq_msg_t msg;
        zmq_msg_init_data(&msg, *m, strlen(*m), NULL, NULL);
        int rc = zmq_send(sock, &msg, *(m+1) ? ZMQ_SNDMORE : 0);
        assert(rc != -1);
    }
    zmq_close(sock);
    zmq_term(ctx);
}

void run_zmq_pull() {
    reader_socket(ZMQ_PULL);
}

void run_zmq_push() {
    writer_socket(ZMQ_PUSH);
}

void run_zmq_sub() {
    reader_socket(ZMQ_SUB);
}

void run_zmq_pub() {
    writer_socket(ZMQ_PUB);
}

void run_zmq_req() {
    void *ctx = open_context();
    assert(ctx);
    void *sock = zmq_socket(ctx, ZMQ_REQ);
    assert(sock);
    configure_socket(sock);
    for(char **m = cli_options.messages; *m; ++m) {
        zmq_msg_t msg;
        zmq_msg_init_data(&msg, *m, strlen(*m), NULL, NULL);
        int rc = zmq_send(sock, &msg, *(m+1) ? ZMQ_SNDMORE : 0);
        assert(rc != -1);
    }
    uint64_t more = 1;
    size_t moresz = sizeof(more);
    while(more) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        int rc = zmq_recv(sock, &msg, 0);
        assert(rc != -1);
        rc = zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &moresz);
        assert(rc != -1);
        print_message(zmq_msg_data(&msg), zmq_msg_size(&msg), more);
        zmq_msg_close(&msg);
    }
    zmq_close(sock);
    zmq_term(ctx);
}

void run_zmq_rep() {
    void *ctx = open_context();
    assert(ctx);
    void *sock = zmq_socket(ctx, ZMQ_REP);
    assert(sock);
    configure_socket(sock);
    while(1) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        int rc = zmq_recv(sock, &msg, 0);
        assert(rc != -1);
        uint64_t more = 0;
        size_t moresz = sizeof(more);
        rc = zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &moresz);
        assert(rc != -1);
        print_message(zmq_msg_data(&msg), zmq_msg_size(&msg), more);
        zmq_msg_close(&msg);

        for(char **m = cli_options.messages; *m; ++m) {
            zmq_msg_t msg;
            zmq_msg_init_data(&msg, *m, strlen(*m), NULL, NULL);
            int rc = zmq_send(sock, &msg, *(m+1) ? ZMQ_SNDMORE : 0);
            assert(rc != -1);
        }
    }
    zmq_close(sock);
    zmq_term(ctx);
}


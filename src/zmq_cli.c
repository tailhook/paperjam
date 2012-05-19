#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

static int do_receive(void *sock, zmq_msg_t *msg) {
    zmq_pollitem_t item = {sock, 0, ZMQ_POLLIN, 0 };
    while(get_time() < cli_options.finishtime) {
        int left = -1;
        if(cli_options.timeout) {
            left = (int)((cli_options.finishtime - get_time())*1000);
        }
        int rc = zmq_poll(&item, 1, left);
        rc = zmq_recv(sock, msg, ZMQ_NOBLOCK);
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
    zmq_pollitem_t item = {sock, 0, ZMQ_POLLOUT, 0 };
    while(get_time() < cli_options.finishtime) {
        int left = -1;
        if(cli_options.timeout) {
            left = (int)((cli_options.finishtime - get_time())*1000);
        }
        int rc = zmq_poll(&item, 1, left);
        zmq_msg_t msg;
        zmq_msg_init_data(&msg, data, len, NULL, NULL);
        rc = zmq_send(sock, &msg, (more ? ZMQ_SNDMORE : 0)|ZMQ_NOBLOCK);
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
    zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(linger));
    zmq_close(sock);
    zmq_term(ctx);
}


static void reader_socket(int type) {
    int rc;
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
        if(do_receive(sock, &msg)) break;
        uint64_t more = 0;
        size_t moresz = sizeof(more);
        rc = zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &moresz);
        assert(rc != -1);
        print_message(zmq_msg_data(&msg), zmq_msg_size(&msg), more);
        zmq_msg_close(&msg);
    }
    do_finish(sock, ctx);
}


static void writer_socket(int type) {
    void *ctx = open_context();
    assert(ctx);
    void *sock = zmq_socket(ctx, type);
    assert(sock);
    configure_socket(sock);
    for(char **m = cli_options.messages; *m; ++m) {
        if(do_send(sock, *m, strlen(*m), *(m+1) ? 1 : 0)) break;
    }
    do_finish(sock, ctx);
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
    int rc;
    void *ctx = open_context();
    assert(ctx);
    void *sock = zmq_socket(ctx, ZMQ_REQ);
    assert(sock);
    configure_socket(sock);
    for(char **m = cli_options.messages; *m; ++m) {
        if(do_send(sock, *m, strlen(*m), *(m+1) ? 1 : 0)) break;
    }
    uint64_t more = 1;
    size_t moresz = sizeof(more);
    while(more) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        if(do_receive(sock, &msg)) break;
        rc = zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &moresz);
        assert(rc != -1);
        print_message(zmq_msg_data(&msg), zmq_msg_size(&msg), more);
        zmq_msg_close(&msg);
    }
    do_finish(sock, ctx);
}

void run_zmq_rep() {
    int rc;
    void *ctx = open_context();
    assert(ctx);
    void *sock = zmq_socket(ctx, ZMQ_REP);
    assert(sock);
    configure_socket(sock);
    while(1) {
        uint64_t more = 1;
        size_t moresz = sizeof(more);
        while(more) {
            zmq_msg_t msg;
            zmq_msg_init(&msg);
            if(do_receive(sock, &msg)) goto end;
            rc = zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &moresz);
            assert(rc != -1);
            print_message(zmq_msg_data(&msg), zmq_msg_size(&msg), more);
            zmq_msg_close(&msg);
        }

        for(char **m = cli_options.messages; *m; ++m) {
            if(do_send(sock, *m, strlen(*m), *(m+1) ? 1 : 0)) goto end;
        }
    }
end:
    do_finish(sock, ctx);
}


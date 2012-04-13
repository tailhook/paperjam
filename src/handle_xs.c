
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

#include <xs.h>

#include "handle_xs.h"

#ifndef HAVE_XS

int open_xs_socket(context *ctx, config_socket_t *sock) {
    fprintf(stderr, "Paperjam was compiled without libxs support\n");
    errno = ENOTSUP;
    return -1;
}

#else

#define XS_MORENWAIT (XS_DONTWAIT|XS_SNDMORE)

static int get_fd(config_socket_t *sock);
static int check_state(config_socket_t *sock);
static int read_message(config_socket_t *sock, message *msg);
static int write_message(config_socket_t *sock, message *msg);
static int write_string(config_socket_t *sock, const char *string, int more);
static int close_socket(config_socket_t *sock);

socket_impl xs_socket_impl = {
    get_fd: get_fd,
    check_state: check_state,
    read_message: read_message,
    write_message: write_message,
    write_string: write_string,
    close: close_socket
};

int open_xs_socket(context *ctx, config_socket_t *sock) {
    if(!ctx->xs_context) {
        ctx->xs_context = xs_init();
        if(!ctx->xs_context)
            return -1;
        int threads = ctx->config->Xs.io_threads;
        if(xs_setctxopt(ctx->xs_context, XS_IO_THREADS,
                        &threads, sizeof(threads)) == -1) {
            xs_term(ctx->xs_context);
            ctx->xs_context = NULL;
            return -1;
        }
        int max_sockets = ctx->config->Xs.max_sockets;
        if(xs_setctxopt(ctx->xs_context, XS_MAX_SOCKETS,
                        &max_sockets, sizeof(max_sockets)) == -1) {
            xs_term(ctx->xs_context);
            ctx->xs_context = NULL;
            return -1;
        }
    }

    int type = 0;
    switch(sock->kind) {
    case CONFIG_xs_Sub:
        type = XS_SUB;
        break;
    case CONFIG_xs_Pub:
        type = XS_PUB;
        break;
    case CONFIG_xs_Req:
        type = XS_XREQ;
        break;
    case CONFIG_xs_Rep:
        type = XS_XREP;
        break;
    case CONFIG_xs_Push:
        type = XS_PUSH;
        break;
    case CONFIG_xs_Pull:
        type = XS_PULL;
        break;
    default:
        assert(0);
        break;
    }
    void *s = xs_socket(ctx->xs_context, type);
    if(!s) return -1;

    CONFIG_ENDPOINT_TYPE_LOOP(addr, sock->value) {
        if(addr->value.kind == CONFIG_Bind) {
            XS_SOCKETERR(sock, xs_bind(s, addr->value.value));
        } else {
            XS_SOCKETERR(sock, xs_connect(s, addr->value.value));
        }
    }
    if(type == XS_SUB) {
        if(sock->subscribe_len) {
            CONFIG_STRING_LOOP(topic, sock->subscribe) {
                XS_SOCKETERR(sock, xs_setsockopt(s, XS_SUBSCRIBE,
                    topic->value, topic->value_len));
            }
        } else {
            XS_SOCKETERR(sock, xs_setsockopt(s, XS_SUBSCRIBE, NULL, 0));
        }
    }
    // TODO(tailhook) set socket options
    sock->_state.socket = s;
    sock->_impl = &xs_socket_impl;
    return 0;
}

static int close_socket(config_socket_t *sock) {
    XS_ASSERTERR(xs_close(sock));
    return 0;
}

static int get_fd(config_socket_t *sock) {
    int fd;
    size_t sz = sizeof(fd);
    int rc = xs_getsockopt(sock->_state.socket, XS_FD, &fd, &sz);
    if(rc < 0) return -1;
    return fd;
}

static int check_state(config_socket_t *sock) {
    int ev;
    size_t sz = sizeof(ev);
    int rc = xs_getsockopt(sock->_state.socket, XS_EVENTS, &ev, &sz);
    if(rc < 0) return -1;
    sock->_state.readable = (ev & XS_POLLIN) ? 1 : 0;
    sock->_state.writeable = (ev & XS_POLLOUT) ? 1 : 0;
    return 0;
}

static int read_message(config_socket_t *sock, message *msg) {
    int rc = xs_msg_init(&msg->xs_msg);
    if(rc < 0) return rc;
    rc = xs_recvmsg(sock->_state.socket, &msg->xs_msg, XS_DONTWAIT);
    if(rc < 0) return rc;
    int more;
    size_t sz = sizeof(more);
    rc = xs_getsockopt(sock->_state.socket, XS_RCVMORE, &more, &sz);
    if(rc < 0) {
        int oldeno = errno;
        XS_ASSERTERR(xs_msg_close(&msg->xs_msg));
        errno = oldeno;
        return rc;
    }
    msg->xs = 1;
    msg->more = more;
    return 0;
}

static int write_message(config_socket_t *sock, message *msg) {
    int rc;
# if HAVE_ZMQ
    if(!msg->xs) {
        assert(msg->zmq);
        // TODO(tailhook) optimize copying
        size_t len = zmq_msg_size(&msg->zmq_msg);
        rc = xs_msg_init_size(&msg->xs_msg, len);
        if(rc < 0) return rc;
        memcpy(xs_msg_data(&msg->xs_msg), zmq_msg_data(&msg->zmq_msg), len);
        msg->xs = 1;
    }
# else
    assert(msg->xs);
# endif
    xs_msg_t tmsg;
    // TODO(tailhook) optimize this copy
    xs_msg_init(&tmsg);
    rc = xs_msg_copy(&tmsg, &msg->xs_msg);
    if(rc < 0) return rc;
    rc = xs_sendmsg(sock->_state.socket, &tmsg,
                    msg->more ? XS_MORENWAIT : XS_DONTWAIT);
    XS_ASSERTERR(xs_msg_close(&tmsg));
    return rc;
}

static int write_string(config_socket_t *sock, const char *str, int more) {
    xs_msg_t tmsg;
    int len = strlen(str);
    int rc = xs_msg_init_size(&tmsg, len);
    if(rc < 0) return rc;
    memcpy(xs_msg_data(&tmsg), str, len);
    rc = xs_sendmsg(sock->_state.socket, &tmsg,
                    more ? XS_MORENWAIT : XS_DONTWAIT);
    XS_ASSERTERR(xs_msg_close(&tmsg));
    return rc;
}

#endif //HAVE_XS



#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include <zmq.h>

#include "handle_zmq.h"

#ifndef HAVE_ZMQ

int open_zmq_socket(context *ctx, config_socket_t *sock) {
    fprintf(stderr, "Paperjam was compiled without libzmq support\n");
    errno = ENOTSUP;
    return -1;
}

#else

#ifndef ZMQ_DONTWAIT
#   define ZMQ_DONTWAIT     ZMQ_NOBLOCK
#endif
#define ZMQ_MORENWAIT   (ZMQ_SNDMORE|ZMQ_DONTWAIT)
#if ZMQ_VERSION_MAJOR == 2
#   define zmq_msg_send(msg, sock, opt) zmq_send (sock, msg, opt)
#   define zmq_msg_recv(msg, sock, opt) zmq_recv (sock, msg, opt)
#   define ZMQ_POLL_MSEC    1000        //  zmq_poll is usec
#   define ZMQ_MORE_TYPE    uint64_t
#   define ZMQ_EVENTS_TYPE  uint32_t
#elif ZMQ_VERSION_MAJOR == 3
#   define ZMQ_POLL_MSEC    1           //  zmq_poll is msec
#   define ZMQ_MORE_TYPE    int
#endif

static int get_fd(config_socket_t *sock);
static int check_state(config_socket_t *sock);
static int read_message(config_socket_t *sock, message *msg);
static int write_message(config_socket_t *sock, message *msg);
static int write_string(config_socket_t *sock, const char *string, int more);
static int close_socket(config_socket_t *sock);

socket_impl zmq_socket_impl = {
    get_fd: get_fd,
    check_state: check_state,
    read_message: read_message,
    write_message: write_message,
    write_string: write_string,
    close: close_socket
};

int open_zmq_socket(context *ctx, config_socket_t *sock) {
    if(!ctx->zmq_context) {
        ctx->zmq_context = zmq_init(ctx->config->Zmq.io_threads);
        if(!ctx->zmq_context)
            return -1;
    }

    int type = 0;
    switch(sock->kind) {
    case CONFIG_zmq_Sub:
        type = ZMQ_SUB;
        break;
    case CONFIG_zmq_Pub:
        type = ZMQ_PUB;
        break;
    case CONFIG_zmq_Req:
        type = ZMQ_DEALER;
        break;
    case CONFIG_zmq_Rep:
        type = ZMQ_ROUTER;
        break;
    case CONFIG_zmq_Push:
        type = ZMQ_PUSH;
        break;
    case CONFIG_zmq_Pull:
        type = ZMQ_PULL;
        break;
    default:
        assert(0);
        break;
    }
    void *s = zmq_socket(ctx->zmq_context, type);
    if(!s) return -1;

    CONFIG_ENDPOINT_TYPE_LOOP(addr, sock->value) {
        if(addr->value.kind == CONFIG_Bind) {
            ZMQ_SOCKETERR(sock, zmq_bind(s, addr->value.value));
        } else {
            ZMQ_SOCKETERR(sock, zmq_connect(s, addr->value.value));
        }
    }
    if(type == ZMQ_SUB) {
        if(sock->subscribe_len) {
            CONFIG_STRING_LOOP(topic, sock->subscribe) {
                ZMQ_SOCKETERR(sock, zmq_setsockopt(s, ZMQ_SUBSCRIBE,
                    topic->value, topic->value_len));
            }
        } else {
            ZMQ_SOCKETERR(sock, zmq_setsockopt(s, ZMQ_SUBSCRIBE, NULL, 0));
        }
    }
    // TODO(tailhook) set socket options
    sock->_state.socket = s;
    sock->_impl = &zmq_socket_impl;
    return 0;
}

static int close_socket(config_socket_t *sock) {
    ZMQ_ASSERTERR(zmq_close(sock));
    return 0;
}

static int get_fd(config_socket_t *sock) {
    int fd;
    size_t sz = sizeof(fd);
    int rc = zmq_getsockopt(sock->_state.socket, ZMQ_FD, &fd, &sz);
    if(rc < 0) return -1;
    return fd;
}

static int check_state(config_socket_t *sock) {
    ZMQ_EVENTS_TYPE ev;
    size_t sz = sizeof(ev);
    int rc = zmq_getsockopt(sock->_state.socket, ZMQ_EVENTS, &ev, &sz);
    if(rc < 0) return -1;
    sock->_state.readable = (ev & ZMQ_POLLIN) ? 1 : 0;
    sock->_state.writeable = (ev & ZMQ_POLLOUT) ? 1 : 0;
    return 0;
}

static int read_message(config_socket_t *sock, message *msg) {
    int rc = zmq_msg_init(&msg->zmq_msg);
    if(rc < 0) return rc;
    rc = zmq_msg_recv(&msg->zmq_msg, sock->_state.socket, ZMQ_DONTWAIT);
    if(rc < 0) return rc;
    ZMQ_MORE_TYPE more;
    size_t sz = sizeof(more);
    rc = zmq_getsockopt(sock->_state.socket, ZMQ_RCVMORE, &more, &sz);
    if(rc < 0) {
        int oldeno = errno;
        ZMQ_ASSERTERR(zmq_msg_close(&msg->zmq_msg));
        errno = oldeno;
        return rc;
    }
    msg->zmq = 1;
    msg->more = more;
    return 0;
}

static int write_message(config_socket_t *sock, message *msg) {
    int rc;
# if HAVE_XS
    if(!msg->zmq) {
        assert(msg->xs);
        // TODO(tailhook) optimize copying
        size_t len = xs_msg_size(&msg->xs_msg);
        rc = zmq_msg_init_size(&msg->zmq_msg, len);
        if(rc < 0) return rc;
        memcpy(zmq_msg_data(&msg->zmq_msg), xs_msg_data(&msg->xs_msg), len);
        msg->zmq = 1;
    }
# else
    assert(msg->zmq);
# endif
    zmq_msg_t tmsg;
    // TODO(tailhook) optimize this copy
    zmq_msg_init(&tmsg);
    rc = zmq_msg_copy(&tmsg, &msg->zmq_msg);
    if(rc < 0) return rc;
    rc = zmq_msg_send(&tmsg, sock->_state.socket,
                      msg->more ? ZMQ_MORENWAIT : ZMQ_DONTWAIT);
    ZMQ_ASSERTERR(zmq_msg_close(&tmsg));
    return rc;
}

static int write_string(config_socket_t *sock, const char *str, int more) {
    zmq_msg_t tmsg;
    int len = strlen(str);
    int rc = zmq_msg_init_size(&tmsg, len);
    if(rc < 0) return rc;
    memcpy(zmq_msg_data(&tmsg), str, len);
    rc = zmq_msg_send(&tmsg, sock->_state.socket,
                      more ? ZMQ_MORENWAIT : ZMQ_DONTWAIT);
    ZMQ_ASSERTERR(zmq_msg_close(&tmsg));
    return rc;
}


#endif //HAVE_ZMQ

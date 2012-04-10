
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include <zmq.h>

#include "handle_zmq.h"
#include "handle_xs.h"

#ifndef HAVE_ZMQ

int init_zmq_context(config_main_t *config) {
    fprintf(stderr, "Paperjam was compiled without libzmq support\n");
    errno = ENOTSUP;
    return -1;
}

int open_zmq_socket(config_socket_t *sock) {
    fprintf(stderr, "Paperjam was compiled without libzmq support\n");
    errno = ENOTSUP;
    return -1;
}

int zmq_poll_start(config_main_t *sock) {
    fprintf(stderr, "Paperjam was compiled without libzmq support\n");
    errno = ENOTSUP;
    return -1;
}

#else

#ifndef ZMQ_DONTWAIT
#   define ZMQ_DONTWAIT     ZMQ_NOBLOCK
#endif
#if ZMQ_VERSION_MAJOR == 2
#   define zmq_msg_send(msg, sock, opt) zmq_send (sock, msg, opt)
#   define zmq_msg_recv(msg, sock, opt) zmq_recv (sock, msg, opt)
#   define ZMQ_POLL_MSEC    1000        //  zmq_poll is usec
#   define ZMQ_MORE_TYPE    uint64_t
#elif ZMQ_VERSION_MAJOR == 3
#   define ZMQ_POLL_MSEC    1           //  zmq_poll is msec
#   define ZMQ_MORE_TYPE    int
#endif

enum {
    WFRONTEND,
    WBACKEND
};

void *zmq_context;

int init_zmq_context(config_main_t *config) {
    zmq_context = zmq_init(config->Zmq.io_threads);
    // TODO(tailhook) implement >3.1 option handling
    if(!zmq_context)
        return -1;
    return 0;
}

int open_zmq_socket(config_socket_t *sock) {
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
    void *s = zmq_socket(zmq_context, type);
    if(!s) return -1;
    CONFIG_ENDPOINT_TYPE_LOOP(addr, sock->value) {
        if(addr->value.kind == CONFIG_Bind) {
            ZMQ_ASSERTERR(zmq_bind(s, addr->value.value));
        } else {
            ZMQ_ASSERTERR(zmq_connect(s, addr->value.value));
        }
    }
    // TODO(tailhook) set socket options
    sock->_socket = s;
    return 0;
}

int zmq_get_fd(config_socket_t *sock) {
    int fd;
    size_t sz = sizeof(fd);
    int rc = zmq_getsockopt(sock->_socket, ZMQ_FD, &fd, &sz);
    if(rc < 0) return -1;
    return fd;
}

static int add_socket(zmq_pollitem_t *pitem, config_socket_t *sock) {
    if(IS_XS(sock)) {
        pitem->fd = xs_get_fd(sock);
        pitem->events = ZMQ_POLLIN;
    } else if(IS_ZMQ(sock)) {
        pitem->socket = sock->_socket;
        if(sock->kind == CONFIG_zmq_Pull ||
           sock->kind == CONFIG_zmq_Rep ||
           sock->kind == CONFIG_zmq_Req ||
           sock->kind == CONFIG_zmq_Sub) {
            pitem->events = ZMQ_POLLIN;
        }
    } else {
        assert(0);
    }
    return 0;
}

static void process_zmq_message(config_socket_t *src, config_socket_t *tgt,
                               config_socket_t *monitor) {
    ZMQ_MORE_TYPE more = 0;
    size_t sz = sizeof(more);

    zmq_msg_t msg;
    ZMQ_ASSERTERR(zmq_msg_init(&msg));
    int rc = zmq_msg_recv(&msg, src->_socket, ZMQ_DONTWAIT);
    if(rc < 0) {
        if(errno == EAGAIN) return;
        ZMQ_SOCKETERR(src, rc);
    }
    ZMQ_SOCKETERR(src, zmq_getsockopt(src->_socket, ZMQ_RCVMORE, &more, &sz));
    rc = zmq_msg_send(&msg, tgt->_socket,
        (more ? ZMQ_SNDMORE : 0) | ZMQ_DONTWAIT);
    if(rc < 0) {
        if(errno == EAGAIN) goto error;
        ZMQ_SOCKETERR(tgt, rc);
    }
    while(more) {
        ZMQ_SOCKETERR(src, zmq_msg_recv(&msg, src->_socket, ZMQ_DONTWAIT));
        ZMQ_SOCKETERR(src, zmq_getsockopt(src->_socket,
            ZMQ_RCVMORE, &more, &sz));
        ZMQ_SOCKETERR(tgt, zmq_msg_send(&msg, tgt->_socket,
            (more ? ZMQ_SNDMORE : 0) | ZMQ_DONTWAIT));
    }
    return;

error:
    ZMQ_ASSERTERR(zmq_msg_close(&msg));
    ZMQ_ASSERTERR(zmq_getsockopt(src, ZMQ_RCVMORE, &more, &sz));
    while(more) {
        ZMQ_ASSERTERR(zmq_msg_recv(&msg, src, ZMQ_DONTWAIT));
        ZMQ_ASSERTERR(zmq_msg_close(&msg));
        ZMQ_ASSERTERR(zmq_getsockopt(src, ZMQ_RCVMORE, &more, &sz));
    }
}


static void process_message(config_socket_t *src, config_socket_t *tgt,
                            config_socket_t *monitor)
{
    // TODO(tailhook) check if tgt is writable
    if(IS_ZMQ(src) && IS_ZMQ(tgt)) {
        process_zmq_message(src, tgt, monitor);
        return;
    }
    assert(0);
}

int zmq_poll_start(config_main_t *config) {
    int pollitems = config->Devices_len*2;
    struct {
        config_device_t *dev;
        int which;
    } devices[pollitems];
    zmq_pollitem_t poll[pollitems];
    memset(poll, sizeof(poll), 0);
    int index = 0;
    CONFIG_STRING_DEVICE_LOOP(item, config->Devices) {
        devices[index].dev = &item->value;
        devices[index].which = WFRONTEND;
        item->value.frontend._pollindex = index;
        XS_ASSERTERR(add_socket(&poll[index++], &item->value.frontend));
        devices[index].dev = &item->value;
        devices[index].which = WBACKEND;
        item->value.backend._pollindex = index;
        XS_ASSERTERR(add_socket(&poll[index++], &item->value.backend));
    }
    while(1) {
        int rc = zmq_poll(poll, pollitems, -1);
        ZMQ_ASSERTERR(rc);
        for(int i = 0;i < pollitems; ++i) {
            if(!poll[i].revents) continue;

            if(poll[i].revents & ZMQ_POLLIN) {
                config_device_t *dev = devices[i].dev;
                int which = devices[i].which;
                // Let's check if we are able to write
                if(which == WFRONTEND) {
                    process_message(&dev->frontend, &dev->backend,
                                    &dev->monitor);
                } else {
                    process_message(&dev->backend, &dev->frontend,
                                    &dev->monitor);
                }
            }
            if(poll[i].revents & ZMQ_POLLOUT) {
                // If we polling for POLLOUT then there is a socket
                // active for reading
                //config_device_t *dev = devices[i];
                assert(0);
            }

            if(!--rc) break;
        }
    }
}

#endif //HAVE_ZMQ

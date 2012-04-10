
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

#include <xs.h>

#include "handle_xs.h"
#include "handle_zmq.h"

#ifndef HAVE_XS

int init_xs_context(config_main_t *config) {
    fprintf(stderr, "Paperjam was compiled without libxs support\n");
    errno = ENOTSUP;
    return -1;
}

int open_xs_socket(config_socket_t *sock) {
    fprintf(stderr, "Paperjam was compiled without libxs support\n");
    errno = ENOTSUP;
    return -1;
}

int xs_poll_start(config_main_t *config) {
    fprintf(stderr, "Paperjam was compiled without libxs support\n");
    errno = ENOTSUP;
    return -1;
}

#else

enum {
    WFRONTEND,
    WBACKEND
};

void *xs_context;

int init_xs_context(config_main_t *config) {
    xs_context = xs_init();
    if(!xs_context)
        return -1;
    int threads = config->Xs.io_threads;
    if(xs_setsockopt(xs_context, XS_IO_THREADS,
                     &threads, sizeof(threads)) == -1) {
        xs_term(xs_context);
        xs_context = NULL;
        return -1;
    }
    int max_sockets = config->Xs.max_sockets;
    if(xs_setsockopt(xs_context, XS_MAX_SOCKETS,
                     &max_sockets, sizeof(max_sockets)) == -1) {
        xs_term(xs_context);
        xs_context = NULL;
        return -1;
    }
    return 0;
}

int open_xs_socket(config_socket_t *sock) {
    int type = 0;
    switch(sock->kind) {
    case CONFIG_xs_Sub:
        type = XS_XSUB;
        break;
    case CONFIG_xs_Pub:
        type = XS_XPUB;
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
    void *s = xs_socket(xs_context, type);
    if(!s) return -1;
    // TODO(tailhook) set socket options
    sock->_socket = s;
    return 0;
}

int xs_get_fd(config_socket_t *sock) {
    int fd;
    size_t sz = sizeof(fd);
    int rc = xs_getsockopt(sock->_socket, XS_FD, &fd, &sz);
    if(rc < 0) return -1;
    return fd;
}

static int add_socket(xs_pollitem_t *pitem, config_socket_t *sock) {
    if((sock->kind & LIB_MASK) == FLAG_LIBZMQ) {
        pitem->fd = zmq_get_fd(sock);
        pitem->events = XS_POLLIN;
    } else if((sock->kind & LIB_MASK) == FLAG_LIBXS) {
        pitem->socket = sock->_socket;
        if(sock->kind == CONFIG_xs_Pull ||
           sock->kind == CONFIG_xs_Rep ||
           sock->kind == CONFIG_xs_Req ||
           sock->kind == CONFIG_xs_Sub) {
            pitem->events = XS_POLLIN;
        }
    } else {
        assert(0);
    }
    return 0;
}

static void process_xs_message(config_socket_t *src, config_socket_t *tgt,
                               config_socket_t *monitor) {
    int more = 0;
    size_t sz = sizeof(more);

    xs_msg_t msg;
    XS_ASSERTERR(xs_msg_init(&msg));
    int rc = xs_recvmsg(src, &msg, XS_DONTWAIT);
    if(rc < 0) {
        if(errno == EAGAIN) return;
        XS_ASSERTERR(rc);
    }
    rc = xs_sendmsg(tgt, &msg, XS_DONTWAIT);
    if(rc < 0) {
        if(errno == EAGAIN) goto error;
        XS_ASSERTERR(rc);
    }
    XS_ASSERTERR(xs_getsockopt(src, XS_RCVMORE, &more, &sz));
    while(more) {
        XS_ASSERTERR(xs_recvmsg(src, &msg, XS_DONTWAIT));
        XS_ASSERTERR(xs_sendmsg(src, &msg, XS_DONTWAIT));
        XS_ASSERTERR(xs_getsockopt(src, XS_RCVMORE, &more, &sz));
    }
    return;

error:
    XS_ASSERTERR(xs_msg_close(&msg));
    XS_ASSERTERR(xs_getsockopt(src, XS_RCVMORE, &more, &sz));
    while(more) {
        XS_ASSERTERR(xs_recvmsg(src, &msg, XS_DONTWAIT));
        XS_ASSERTERR(xs_msg_close(&msg));
        XS_ASSERTERR(xs_getsockopt(src, XS_RCVMORE, &more, &sz));
    }
}


static void process_message(config_socket_t *src, config_socket_t *tgt,
                            config_socket_t *monitor)
{
    // TODO(tailhook) check if tgt is writable
    if(IS_XS(src) && IS_XS(tgt)) {
        process_xs_message(src, tgt, monitor);
    }
    assert(0);
}

int xs_poll_start(config_main_t *config) {
    int pollitems = config->Devices_len*2;
    struct {
        config_device_t *dev;
        int which;
    } devices[pollitems];
    xs_pollitem_t poll[pollitems];
    memset(poll, sizeof(poll), 0);
    int index = 0;
    CONFIG_STRING_DEVICE_LOOP(item, config->Devices) {
        devices[index].dev = &item->value;
        devices[index].which = WFRONTEND;
        item->value.frontend._pollindex = index;
        add_socket(&poll[index++], &item->value.frontend);
        devices[index].dev = &item->value;
        devices[index].which = WBACKEND;
        item->value.backend._pollindex = index;
        add_socket(&poll[index++], &item->value.backend);
    }
    while(1) {
        int rc = xs_poll(poll, pollitems, 0);
        XS_ASSERTERR(rc);
        for(int i = 0;i < pollitems; ++i) {
            if(!poll[i].revents) continue;

            if(poll[i].revents & XS_POLLIN) {
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
            if(poll[i].revents & XS_POLLOUT) {
                // If we polling for POLLOUT then there is a socket
                // active for reading
                //config_device_t *dev = devices[i];
                assert(0);
            }

            if(!--rc) break;
        }
    }
}

#endif //HAVE_XS


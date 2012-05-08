#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <poll.h>

#include "config.h"
#include "handle_zmq.h"
#include "handle_xs.h"
#include "devices.h"

#define ASSERTERR(val) if((val) == -1) { \
    fprintf(stderr, "Assertion error at %s:%d: %s\n", \
        __FILE__, __LINE__, \
        strerror(errno)); \
    abort(); \
    }

static int close_message(message *msg) {
    // TODO(tailhook)
    if(msg->zmq) {
#ifdef HAVE_ZMQ
        ZMQ_ASSERTERR(zmq_msg_close(&msg->zmq_msg));
#else
        assert(0);
#endif
    }
    if(msg->xs) {
#ifdef HAVE_XS
        XS_ASSERTERR(xs_msg_close(&msg->xs_msg));
#else
        assert(0);
#endif
    }
    msg->xs = 0;
    msg->zmq = 0;
    msg->more = 0;
    return 0;
}

static int printmsg(context *ctx, config_socket_t *mon)
{
    message msg;
    msg.xs = 0;
    msg.zmq = 0;
    msg.more = 0;
    int rc = mon->_impl->read_message(mon, &msg);
    if(rc == -1) {
        if(errno == EAGAIN) {
            mon->_state.readable = 0;
            return 0;
        }
        ASSERTERR(rc);
    }
    if(msg.xs) {
#ifdef HAVE_XS
        printf("[%s] ``%.*s''", mon->_name,
            (int)xs_msg_size(&msg.xs_msg),
            (char *)xs_msg_data(&msg.xs_msg));
#endif
    } else if(msg.zmq) {
#ifdef HAVE_ZMQ
        printf("[%s] ``%.*s''", mon->_name,
            (int)zmq_msg_size(&msg.zmq_msg),
            (char *)zmq_msg_data(&msg.zmq_msg));
#endif
    }
    while(msg.more) {
        close_message(&msg);
        ASSERTERR(mon->_impl->read_message(mon, &msg));
        if(msg.xs) {
#ifdef HAVE_XS
            printf(", ``%.*s''",
                (int)xs_msg_size(&msg.xs_msg),
                (char *)xs_msg_data(&msg.xs_msg));
#endif
        } else if(msg.zmq) {
#ifdef HAVE_ZMQ
            printf(", ``%.*s''",
                (int)zmq_msg_size(&msg.zmq_msg),
                (char *)zmq_msg_data(&msg.zmq_msg));
#endif
        }
    }
    printf("\n");
    fflush(stdout);
    close_message(&msg);
    return 1;
}

static int iterate(context *ctx) {
    int res = 0;  // total number of messages done
    int iter;  // messages on single socket pair
    int rc;
    CONFIG_STRING_DEVICE_LOOP(item, ctx->config->Devices) {
        if(item->value.monitor._state.readable) {
            iter = 0;
            do {
                iter += 1;
                rc = printmsg(ctx, &item->value.monitor);
                assert(rc >= 0);
            } while(rc && iter < 100);
            if(iter) {
                item->value.monitor._impl->check_state(&item->value.frontend);
                res = 1;
            }
        }
    }
    return res;
}

void loop(context *ctx) {
    int maxpoll = ctx->config->Devices_len*2;
    int nsocks = 0;
    struct pollfd pollitems[maxpoll];
    config_socket_t *sockets[maxpoll];

    CONFIG_STRING_DEVICE_LOOP(item, ctx->config->Devices) {
        config_socket_t *sock = &item->value.monitor;
        pollitems[nsocks].fd = sock->_impl->get_fd(sock);
        pollitems[nsocks].events = POLLIN;
        sockets[nsocks] = sock;
        nsocks += 1;
        sock->_impl->check_state(sock);
    }
    int timeout = 0;
    for(;;) {
        int rc = poll(pollitems, nsocks, timeout);
        assert(rc >= 0); // remove
        if(rc > 0) {
            for(int i = 0; i < nsocks; ++i) {
                if(pollitems[i].revents) {
                    sockets[i]->_impl->check_state(sockets[i]);
                }
                // TODO(tailhook) optimize if rc < nsocks
            }
        }
        if(iterate(ctx)) {
            // there is something to process, but we want to check other
            // sockets too
            timeout = 0;
        } else {
            // nothing to do, can wait indefinitely
            timeout = -1;
        }
    }

}


int main(int argc, char **argv) {

    config_main_t *config = malloc(sizeof(config_main_t));
    assert(config);
    config_load(config, argc, argv);

    int zmq_socks = 0;
    int xs_socks = 0;
    context ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.config = config;

    CONFIG_STRING_DEVICE_LOOP(item, config->Devices) {
        item->value.monitor._name = item->key;
        item->value.monitor._type = "monitor";
        if(item->value.monitor.kind) {
            switch(item->value.monitor.kind) {
            case CONFIG_zmq_Push:
                item->value.monitor.kind = CONFIG_zmq_Pull;
                break;
            case CONFIG_zmq_Pub:
                item->value.monitor.kind = CONFIG_zmq_Sub;
                break;
            case CONFIG_xs_Push:
                item->value.monitor.kind = CONFIG_xs_Pull;
                break;
            case CONFIG_xs_Pub:
                item->value.monitor.kind = CONFIG_xs_Sub;
                break;
            default:
                fprintf(stderr, "Wrong monitor socket for %s", item->key);
                abort();
            }
            CONFIG_ENDPOINT_TYPE_LOOP(addr, item->value.monitor.value) {
                if(addr->value.kind == CONFIG_Bind) {
                    addr->value.kind = CONFIG_Connect;
                } else {
                    addr->value.kind = CONFIG_Bind;
                }
            }
            if(IS_XS(&item->value.monitor)) {
                xs_socks += 1;
                XS_SOCKETERR(&item->value.monitor,
                    open_xs_socket(&ctx, &item->value.monitor));
            } else {
                zmq_socks += 1;
                ZMQ_SOCKETERR(&item->value.monitor,
                    open_zmq_socket(&ctx, &item->value.monitor));
            }
        }

    }

    loop(&ctx);

    CONFIG_STRING_DEVICE_LOOP(item, config->Devices) {
        if(item->value.monitor._impl) {
            item->value.monitor._impl->close(&item->value.monitor);
        }
    }

#ifdef HAVE_XS
    if(ctx.xs_context) {
        XS_ASSERTERR(xs_term(ctx.xs_context));
    }
#endif
#ifdef HAVE_ZMQ
    if(ctx.zmq_context) {
        ZMQ_ASSERTERR(zmq_term(ctx.zmq_context));
    }
#endif
}

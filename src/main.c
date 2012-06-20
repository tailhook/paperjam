#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <poll.h>

#include "config.h"
#include "handle_zmq.h"
#include "handle_xs.h"
#include "paperjam.h"

#define ASSERTERR(val) if((val) == -1) { \
    fprintf(stderr, "Assertion error at %s:%d: %s\n", \
        __FILE__, __LINE__, \
        strerror(errno)); \
    abort(); \
    }

static void device_check(config_socket_t *front, config_socket_t *back) {
    if(IS_READONLY(front) && IS_WRITEONLY(back)) return;
    if(IS_WRITEONLY(front) && IS_READONLY(back)) return;
    if(IS_REQ(front) && IS_REP(back)) return;
    if(IS_REP(front) && IS_REQ(back)) return;
    fprintf(stderr, "Wrong combination of sockets for %s", front->_name);
    abort();
}

static void monitor_check(config_socket_t *sock) {
    if(IS_WRITEONLY(sock)) return;
    // Can reply socket be able to monitor? probably not
    fprintf(stderr, "Wrong socket for monitoring %s, use push or pub sockets",
        sock->_name);
    abort();
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

static int forward(context *ctx, config_socket_t *src, config_socket_t *tgt,
                   config_device_t *dev, int mode)
{
    message msg;
    msg.xs = 0;
    msg.zmq = 0;
    msg.more = 0;
    int rc = src->_impl->read_message(src, &msg);
    if(rc == -1) {
        if(errno == EAGAIN) {
            src->_state.readable = 0;
            return 0;
        }
        ASSERTERR(rc);
    }
    if(mode) {
        dev->_stat.input_msg += 1;
    } else {
        dev->_stat.output_msg += 1;
    }
    int monactive = 0;
    if(dev->monitor.kind) {
        rc = dev->monitor._impl->write_string(&dev->monitor,
            mode ? "in" : "out", 1);
        if(rc >= 0) {
             monactive = 1;
        }
    }
    if(monactive) {
        ASSERTERR(dev->monitor._impl->write_message(&dev->monitor, &msg));
    }
    rc = tgt->_impl->write_message(tgt, &msg);
    if(rc == -1) {
        if(errno == EAGAIN) {
            dev->_stat.discard_msg += 1;
            while(msg.more) {
                close_message(&msg);
                ASSERTERR(src->_impl->read_message(src, &msg));
            }
            close_message(&msg);
            return 0;
        }
        ASSERTERR(rc);
    }
    while(msg.more) {
        close_message(&msg);
        ASSERTERR(src->_impl->read_message(src, &msg));
        if(monactive) {
            ASSERTERR(dev->monitor._impl->write_message(&dev->monitor, &msg));
        }
        ASSERTERR(tgt->_impl->write_message(tgt, &msg));
    }
    close_message(&msg);
    return 1;
}

static int iterate(context *ctx) {
    int res = 0;
    int any;
    int rc;
    CONFIG_STRING_DEVICE_LOOP(item, ctx->config->Devices) {
        any = 0;
        if(item->value.frontend._state.readable
           && item->value.backend._state.writeable) {
            any = 1;
            rc = forward(ctx,
                &item->value.frontend,
                &item->value.backend,
                &item->value, 1);
            assert(rc >= 0);
        }
        if(item->value.backend._state.readable
           && item->value.frontend._state.writeable) {
            any = 1;
            rc = forward(ctx,
                &item->value.backend,
                &item->value.frontend,
                &item->value, 0);
            assert(rc >= 0);
        }
        if(any) {
            item->value.frontend._impl->check_state(&item->value.frontend);
            item->value.backend._impl->check_state(&item->value.backend);
            res = 1;
        }
    }
    return res;
}

static int check_statistics(context *ctx, int timeout) {
    if(!ctx->stat_interval)
        return timeout;

    struct timeval curtime;
    gettimeofday(&curtime, NULL);
    int64_t time = (int64_t)curtime.tv_sec * 1000 + curtime.tv_usec / 1000;
    int64_t diff = ctx->nextstat - time;
    if(diff < 0) {
        char tstamp[32];
        struct tm tm;
        gmtime_r(&curtime.tv_sec, &tm);
        strftime(tstamp, 32, "%Y-%m-%dT%H:%M:%SZ", &tm);

        config_socket_t *statsock = &ctx->config->Estp.socket;
        char buf[512];
        sprintf(buf,
            "ESTP:%.64s:paperjam::devices: %s %ld %lu",
            ctx->config->Estp.hostname,
            tstamp,
            ctx->config->Estp.interval,
            ctx->config->Devices_len);
        statsock->_impl->write_string(statsock, buf, 0);
        CONFIG_STRING_DEVICE_LOOP(item, ctx->config->Devices) {
            if(IS_READABLE(&item->value.frontend)) {
                sprintf(buf,
                    "ESTP:%.64s:paperjam:%.64s:input.messages:  %s %ld %lu:c",
                    ctx->config->Estp.hostname,
                    item->value.frontend._name,
                    tstamp,
                    ctx->config->Estp.interval,
                    item->value._stat.input_msg);
                statsock->_impl->write_string(statsock, buf, 0);
            }
            if(IS_WRITEABLE(&item->value.frontend)) {
                sprintf(buf,
                    "ESTP:%.64s:paperjam:%.64s:output.messages: %s %ld %lu:c",
                    ctx->config->Estp.hostname,
                    item->value.frontend._name,
                    tstamp,
                    ctx->config->Estp.interval,
                    item->value._stat.output_msg);
                statsock->_impl->write_string(statsock, buf, 0);
            }
            sprintf(buf,
                "ESTP:%.64s:paperjam:%.64s:discard.messages: %s %ld %lu:c",
                ctx->config->Estp.hostname,
                item->value.frontend._name,
                tstamp,
                ctx->config->Estp.interval,
                item->value._stat.discard_msg);
            statsock->_impl->write_string(statsock, buf, 0);
        }
        ctx->nextstat = time - time % ctx->stat_interval + ctx->stat_interval;
        diff = ctx->nextstat - time;
    }
    if(timeout == 0)
        return 0;
    return diff;
}

void loop(context *ctx) {
    int maxpoll = ctx->config->Devices_len*2;
    int nsocks = 0;
    struct pollfd pollitems[maxpoll];
    config_socket_t *sockets[maxpoll];

    CONFIG_STRING_DEVICE_LOOP(item, ctx->config->Devices) {
        config_socket_t *sock = &item->value.frontend;
        pollitems[nsocks].fd = sock->_impl->get_fd(sock);
        pollitems[nsocks].events = POLLIN;
        sockets[nsocks] = sock;
        nsocks += 1;
        sock->_impl->check_state(sock);

        sock = &item->value.backend;
        pollitems[nsocks].fd = sock->_impl->get_fd(sock);
        pollitems[nsocks].events = POLLIN;
        sockets[nsocks] = sock;
        nsocks += 1;
        sock->_impl->check_state(sock);
    }

    int timeout = 0;
    for(;;) {
        timeout = check_statistics(ctx, timeout);
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

    config_main_t *config = config_load(NULL, argc, argv);
    assert(config);

    int zmq_socks = 0;
    int xs_socks = 0;
    context ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.config = config;
    if(config->Estp.socket.kind && config->Estp.interval) {
        if(!IS_WRITEONLY(&config->Estp.socket)) {
            fprintf(stderr, "Wrong socket for ESTP");
            abort();
        }
        ctx.stat_interval = config->Estp.interval * 1000; //  milliseconds
    }

    if(config->Estp.socket.kind) {
        if(IS_XS(&config->Estp.socket)) {
            xs_socks += 1;
            XS_SOCKETERR(&config->Estp.socket,
                open_xs_socket(&ctx, &config->Estp.socket));
        } else {
            zmq_socks += 1;
            ZMQ_SOCKETERR(&config->Estp.socket,
                open_zmq_socket(&ctx, &config->Estp.socket));
        }
    }


    CONFIG_STRING_DEVICE_LOOP(item, config->Devices) {
        item->value.frontend._name = item->key;
        item->value.frontend._type = "frontend";
        if(IS_XS(&item->value.frontend)) {
            xs_socks += 1;
            XS_SOCKETERR(&item->value.frontend,
                open_xs_socket(&ctx, &item->value.frontend));
        } else {
            zmq_socks += 1;
            ZMQ_SOCKETERR(&item->value.frontend,
                open_zmq_socket(&ctx, &item->value.frontend));
        }

        item->value.backend._name = item->key;
        item->value.backend._type = "backend";
        if(IS_XS(&item->value.backend)) {
            xs_socks += 1;
            XS_SOCKETERR(&item->value.backend,
                open_xs_socket(&ctx, &item->value.backend));
        } else {
            zmq_socks += 1;
            ZMQ_SOCKETERR(&item->value.backend,
                open_zmq_socket(&ctx, &item->value.backend));
        }
        device_check(&item->value.frontend, &item->value.backend);

        item->value.monitor._name = item->key;
        item->value.monitor._type = "monitor";
        if(item->value.monitor.kind) {
            if(IS_XS(&item->value.monitor)) {
                xs_socks += 1;
                XS_SOCKETERR(&item->value.monitor,
                    open_xs_socket(&ctx, &item->value.monitor));
            } else {
                zmq_socks += 1;
                ZMQ_SOCKETERR(&item->value.monitor,
                    open_zmq_socket(&ctx, &item->value.monitor));
            }
            monitor_check(&item->value.monitor);
        }

    }

    loop(&ctx);

    CONFIG_STRING_DEVICE_LOOP(item, config->Devices) {
        item->value.frontend._impl->close(&item->value.frontend);
        item->value.backend._impl->close(&item->value.backend);
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

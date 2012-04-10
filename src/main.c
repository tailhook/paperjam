#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "config.h"
#include "handle_zmq.h"
#include "handle_xs.h"
#include "devices.h"

static void open_socket(config_socket_t *sock) {
    if(sock->kind == CONFIG_unknown) {
        sock->_socket = NULL;
    } else if(IS_ZMQ(sock)) {
        ZMQ_ASSERTERR(open_zmq_socket(sock));
    } else if(IS_XS(sock)) {
        XS_ASSERTERR(open_xs_socket(sock));
    } else {
        assert(0);
    }
}

static void device_check(config_socket_t *front, config_socket_t *back) {
    // TODO(tailhook) check if socket match by a pattern
}

static void monitor_check(config_socket_t *sock) {
    // TODO(tailhook) check if monitor socket type is ok
}


int main(int argc, char **argv) {

    config_main_t *config = malloc(sizeof(config_main_t));
    assert(config);
    config_load(config, argc, argv);

    int zmq_socks = 0;
    int xs_socks = 0;

    CONFIG_STRING_DEVICE_LOOP(item, config->Devices) {
        if(IS_ZMQ(&item->value.frontend) || IS_ZMQ(&item->value.backend) ||
            IS_ZMQ(&item->value.monitor)) {
            ZMQ_ASSERTERR(init_zmq_context(config));
            zmq_socks += 1;
            break;
        }
    }

    CONFIG_STRING_DEVICE_LOOP(item, config->Devices) {
        if(IS_XS(&item->value.frontend) || IS_XS(&item->value.backend) ||
            IS_XS(&item->value.monitor)) {
            XS_ASSERTERR(init_xs_context(config));
            xs_socks += 1;
            break;
        }
    }

    CONFIG_STRING_DEVICE_LOOP(item, config->Devices) {
        item->value.frontend._name = item->key;
        item->value.frontend._type = "frontend";
        open_socket(&item->value.frontend);

        item->value.backend._name = item->key;
        item->value.backend._type = "backend";
        open_socket(&item->value.backend);

        item->value.monitor._name = item->key;
        item->value.monitor._type = "monitor";
        open_socket(&item->value.monitor);

        device_check(&item->value.frontend, &item->value.backend);
        monitor_check(&item->value.monitor);
    }

    if(xs_socks > zmq_socks) {
        XS_ASSERTERR(xs_poll_start(config));
    } else {
        ZMQ_ASSERTERR(zmq_poll_start(config));
    }

    // TODO(tailhook) implement shutdown code

}

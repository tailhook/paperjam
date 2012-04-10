#ifndef _H_HANDLE_ZMQ_
#define _H_HANDLE_ZMQ_

#include "common.h"
#include "devices.h"

#define FLAG_LIBZMQ 0x100
#define IS_ZMQ(sock) (((sock)->kind & LIB_MASK) == FLAG_LIBZMQ)

#if HAVE_ZMQ
#include <zmq.h>

#define ZMQ_SOCKETERR(sock, val) if((val) == -1) { \
    fprintf(stderr, "Error on ``%s.%s'' at %s:%d: %s\n", \
        (sock)->_name, (sock)->_type, __FILE__, __LINE__, \
        zmq_strerror(errno)); \
    abort(); \
    }
#define ZMQ_ASSERTERR(val) if((val) == -1) { \
    fprintf(stderr, "Assertion error at %s:%d: %s\n", \
        __FILE__, __LINE__, \
        zmq_strerror(errno)); \
    abort(); \
    }

#else

#define ZMQ_ASSERTERR(val) assert((val) != -1);

#endif


int init_zmq_context(config_main_t *cfg);
int open_zmq_socket(config_socket_t *sock);
int zmq_poll_start(config_main_t *cfg);
int zmq_get_fd(config_socket_t *sock);

#endif //_H_HANDLE_ZMQ_

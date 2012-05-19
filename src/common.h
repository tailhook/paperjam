#ifndef _H_COMMON_
#define _H_COMMON_

#include "config.h"
#include CONFIG_HEADER

#ifdef HAVE_XS
#include <xs.h>
#endif
#ifdef HAVE_ZMQ
#include <zmq.h>
#endif

#define LIB_MASK 0xf00
#define IS_READONLY(sock) ((sock)->kind == CONFIG_zmq_Pull \
                           || (sock)->kind == CONFIG_zmq_Sub \
                           || (sock)->kind == CONFIG_xs_Sub \
                           || (sock)->kind == CONFIG_xs_Pull \
                           )
#define IS_READABLE(sock) (IS_READONLY(sock) || IS_REQ(sock) || IS_REP(sock))
#define IS_WRITEONLY(sock) ((sock)->kind == CONFIG_zmq_Push \
                           || (sock)->kind == CONFIG_zmq_Pub \
                           || (sock)->kind == CONFIG_xs_Pub \
                           || (sock)->kind == CONFIG_xs_Push \
                           )
#define IS_WRITEABLE(sock) (IS_WRITEONLY(sock) || IS_REQ(sock) || IS_REP(sock))
#define IS_REQ(sock) ((sock)->kind == CONFIG_zmq_Req \
                           || (sock)->kind == CONFIG_xs_Req \
                           || (sock)->kind == CONFIG_xs_Surveyor \
                           )
#define IS_REP(sock) ((sock)->kind == CONFIG_zmq_Rep \
                           || (sock)->kind == CONFIG_xs_Rep \
                           || (sock)->kind == CONFIG_xs_Respondent \
                           )

typedef struct message {
    int more;
    int xs;
    int zmq;
#ifdef HAVE_XS
    xs_msg_t xs_msg;
#endif
#ifdef HAVE_ZMQ
    zmq_msg_t zmq_msg;
#endif
} message;

typedef struct context {
    config_main_t *config;

    void *xs_context;
    void *zmq_context;
} context;

#endif // _H_COMMON_

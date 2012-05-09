#ifndef _H_HANDLE_XS_
#define _H_HANDLE_XS_

#include "common.h"

#define FLAG_LIBXS 0x200
#define IS_XS(sock) (((sock)->kind & LIB_MASK) == FLAG_LIBXS)

#if HAVE_XS

#include <xs.h>

#define XS_ASSERTERR(val) if((val) == -1) { \
    fprintf(stderr, "Assertion error" #val ": %s\n", xs_strerror(errno)); \
    abort(); \
    }
#define XS_SOCKETERR(sock, val) if((val) == -1) { \
    fprintf(stderr, "Error on ``%s.%s'' at %s:%d: %s\n", \
        (sock)->_name, (sock)->_type, __FILE__, __LINE__, \
        xs_strerror(errno)); \
    abort(); \
    }


#else

#define XS_ASSERTERR(val) if((val) == -1) { \
    fprintf(stderr, "Assertion error" #val ": %s\n", strerror(errno)); \
    abort(); \
    }
#define XS_SOCKETERR(sock, val) if((val) == -1) { \
    fprintf(stderr, "Error on ``%s.%s'' at %s:%d: %s\n", \
        (sock)->_name, (sock)->_type, __FILE__, __LINE__, \
        strerror(errno)); \
    abort(); \
    }

#endif


int open_xs_socket(context *, config_socket_t *sock);

#endif //_H_HANDLE_XS_


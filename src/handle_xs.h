#ifndef _H_HANDLE_XS_
#define _H_HANDLE_XS_

#include "devices.h"
#include "common.h"

#define FLAG_LIBXS 0x200
#define IS_XS(sock) (((sock)->kind & LIB_MASK) == FLAG_LIBXS)

#if HAVE_XS

#include <xs.h>

#define XS_ASSERTERR(val) if((val) == -1) { \
    fprintf(stderr, "Assertion error" #val ": %s\n", xs_strerror(errno)); \
    abort(); \
    }


#else

#define XS_ASSERTERR(val) assert((val) != -1);

#endif

int init_xs_context(config_main_t *config);
int open_xs_socket(config_socket_t *sock);
int xs_poll_start(config_main_t *cfg);
int xs_get_fd(config_socket_t *sock);


#endif //_H_HANDLE_XS_


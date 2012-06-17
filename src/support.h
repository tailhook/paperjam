#ifndef _H_SUPPORT_
#define _H_SUPPORT_

typedef struct config_socket_s config_socket_t;
typedef struct context context;
typedef struct message message;

typedef struct socket_impl {
    int (*close)(config_socket_t *sock);
    int (*get_fd)(config_socket_t *sock);
    int (*check_state)(config_socket_t *sock);
    int (*write_string)(config_socket_t *sock, const char *string, int more);
    int (*write_message)(config_socket_t *sock, message *msg);
    int (*read_message)(config_socket_t *sock, message *msg);
} socket_impl;

typedef struct socket_state {
    void *socket;
    int pollindex;
    int readable;
    int writeable;
} socket_state;

typedef struct device_stat {
    size_t input_msg;
    size_t output_msg;
    size_t discard_msg;
} device_stat;

#endif // _H_SUPPORT_


#ifndef _H_SUPPORT_
#define _H_SUPPORT_

struct config_socket_s;
struct message;
struct context;

typedef struct socket_impl {
    int (*close)(struct config_socket_s *sock);
    int (*get_fd)(struct config_socket_s *sock);
    int (*check_state)(struct config_socket_s *sock);
    int (*write_string)(struct config_socket_s *sock, const char *string, int more);
    int (*write_message)(struct config_socket_s *sock, struct message *msg);
    int (*read_message)(struct config_socket_s *sock, struct message *msg);
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


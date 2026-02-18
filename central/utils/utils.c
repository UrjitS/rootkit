#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>


_Atomic int exit_flag = false;

unsigned short checksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int sum = 0;

    for (; len > 1; len -= 2)
    {
        sum += *buf++;
    }

    if (len == 1)
    {
        sum += *(unsigned char *)buf;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return (unsigned short)(~sum);
}

void log_message(const char *format, ...) {
    // Get current time
    time_t now;
    time(&now);
    const struct tm * local = localtime(&now);

    // Print timestamp
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local);
    printf("%s : ", time_str);

    // Handle variable arguments
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");

    fflush(stdout);
}

void print_linked_list(const struct packet_data * head) {
    const struct packet_data * packet_data = head;
    if (packet_data->next == NULL) {
        log_message("Node data: %d", packet_data->data);
        return;
    }

    while (packet_data->next != NULL) {
        log_message("Node data: %d", packet_data->data);
        packet_data = packet_data->next;
    }

    log_message("Node data: %d", packet_data->data);
}

void free_linked_list(struct packet_data * head) {
    struct packet_data * packet_data = head;
    if (packet_data->next == NULL) {
        free(packet_data);
        return;
    }

    while (packet_data->next != NULL) {
        struct packet_data *previous_node = packet_data;
        packet_data = packet_data->next;
        free(previous_node);
    }

    free(packet_data);
}

int generate_random_length(const int max_length) {
    return rand() % (max_length + 1);
}

char random_char(const int index) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    return charset[index];
}

char * generate_random_string(const size_t length) {
    srand(time(NULL));
    char * dest = malloc(length);
    int i;

    for (i = 0; i < length - 1; i++) {
        const int index = rand() % 62;
        dest[i] = random_char(index);
    }

    dest[i] = '\0';

    return dest;
}

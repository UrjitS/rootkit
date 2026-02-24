#include "utils.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>


_Atomic int exit_flag = false;

static bool rng_seeded = false;

static void ensure_rng_seeded(void) {
    if (!rng_seeded) {
        srand((unsigned int)time(NULL));
        rng_seeded = true;
    }
}

bool parse_int(const char *s, int *out)
{
    char *end = NULL;
    errno = 0;

    const long v = strtol(s, &end, 10);

    if (errno != 0 || end == s || *end != '\0' || v < 0 || v > 65535)
        return false;

    *out = (int)v;
    return true;
}

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
    while (packet_data != NULL) {
        log_message("Node seq: %d, data: %d", packet_data->sequence_number, packet_data->data);
        packet_data = packet_data->next;
    }
}


void free_linked_list(struct packet_data * head) {
    struct packet_data * packet_data = head;
    while (packet_data != NULL) {
        struct packet_data * next = packet_data->next;
        free(packet_data);
        packet_data = next;
    }
}

int generate_random_length(const int max_length) {
    ensure_rng_seeded();
    const int random_number = rand() % (max_length + 1);
    if (random_number == 0) {
        return random_number + 1;
    }
    return random_number;
}

char random_char(const int index) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    return charset[index];
}

char * generate_random_string(const size_t length) {
    if (length == 0) return NULL;

    ensure_rng_seeded();
    char * dest = malloc(length);
    int i;

    for (i = 0; i < length - 1; i++) {
        const int index = rand() % 62;
        dest[i] = random_char(index);
    }

    dest[i] = '\0';

    return dest;
}


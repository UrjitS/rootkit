#include "utils.h"
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>


_Atomic int exit_flag = false;
_Atomic int connection_loop = false;

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

void create_parent_directories(const char * path) {
    char tmp[RESPONSE_BUFFER_LENGTH];
    strncpy(tmp, path, RESPONSE_BUFFER_LENGTH - 1);
    tmp[RESPONSE_BUFFER_LENGTH - 1] = '\0';

    // Walk through each path component and create directories as needed
    for (char * p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) < 0 && errno != EEXIST) {
                log_message("Failed to create directory: %s", tmp);
            }
            *p = '/';
        }
    }
}

int is_all_digits(const char * string) {
    for (const char * p = string; *p; p++) {
        if (!isdigit((unsigned char)*p)) return 0;
    }
    return 1;
}

void try_read_text(const char * path, char * buf, const size_t buflen) {
    buf[0] = '\0';
    FILE * f = fopen(path, "r");
    if (f == NULL) return;
    if (fgets(buf, (int)buflen, f) != NULL) {
        const size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        }
    }
    fclose(f);
}

void try_read_argv0(const char * path, char * buf, const size_t buflen) {
    buf[0] = '\0';
    FILE * f = fopen(path, "rb");
    if (f == NULL) return;

    size_t i = 0;
    int c;
    while (i < buflen - 1 && (c = fgetc(f)) != EOF && c != '\0') {
        buf[i++] = (char)c;
    }
    buf[i] = '\0';
    fclose(f);
}

void try_readlink(const char * path, char * buf, const size_t buflen) {
    buf[0] = '\0';
    const ssize_t len = readlink(path, buf, buflen - 1);
    if (len >= 0) {
        buf[len] = '\0';
    }
}

int collect_processes(process_record_t * records, const int max_records) {
    DIR * proc_dir = opendir("/proc");
    if (proc_dir == NULL) return 0;

    int count = 0;
    struct dirent * entry;

    while ((entry = readdir(proc_dir)) != NULL && count < max_records) {
        if (!is_all_digits(entry->d_name)) continue;

        process_record_t * rec = &records[count];
        rec->pid = atoi(entry->d_name);

        char path[MAX_PROC_PATH];

        snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);
        try_read_text(path, rec->comm, sizeof(rec->comm));

        snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
        try_read_argv0(path, rec->argv0, sizeof(rec->argv0));

        snprintf(path, sizeof(path), "/proc/%s/exe", entry->d_name);
        try_readlink(path, rec->exe, sizeof(rec->exe));

        count++;
    }
    closedir(proc_dir);
    return count;
}

const char * get_base_name(const char * path) {
    const char * slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

void find_most_common_name(const process_record_t * records, const int count, char * result, const size_t result_len) {
    name_count_t counts[MAX_PROCESSES * 3] = {0};
    int num_names = 0;

    for (int i = 0; i < count; i++) {
        const char * candidates[3] = {
            records[i].comm,
            get_base_name(records[i].argv0),
            get_base_name(records[i].exe)
        };

        for (int j = 0; j < 3; j++) {
            const char * name = candidates[j];
            if (name == NULL || name[0] == '\0') continue;
            int found = 0;
            for (int k = 0; k < num_names; k++) {
                if (strcmp(counts[k].name, name) == 0) {
                    counts[k].count++;
                    found = 1;
                    break;
                }
            }

            if (!found && num_names < (int)(sizeof(counts) / sizeof(counts[0]))) {
                strncpy(counts[num_names].name, name, MAX_NAME_LEN - 1);
                counts[num_names].name[MAX_NAME_LEN - 1] = '\0';
                counts[num_names].count = 1;
                num_names++;
            }
        }
    }

    int best_count = 0;
    const char * best_name  = NULL;

    for (int i = 0; i < num_names; i++) {
        if (counts[i].count > best_count) {
            best_count = counts[i].count;
            best_name  = counts[i].name;
        }
    }

    if (best_name != NULL) {
        strncpy(result, best_name, result_len - 1);
        result[result_len - 1] = '\0';
    } else {
        result[0] = '\0';
    }
}
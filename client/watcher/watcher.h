#ifndef ROOTKIT_WATCHER_H
#define ROOTKIT_WATCHER_H

#define MAX_WATCHES 1024
#define MAX_FILES 8192
#define EVENT_BUF_SIZE (1024 * (sizeof(struct inotify_event) + 16))
#define INOTIFY_FLAGS (IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB | IN_Q_OVERFLOW)
#define DEBOUNCE_MS 200

typedef struct {
    char path[RESPONSE_BUFFER_LENGTH];
    time_t mtime;
    off_t size;
} file_entry_t;

typedef struct {
    file_entry_t entries[MAX_FILES];
    int count;
} snapshot_t;

typedef struct {
    int wd;
    char path[RESPONSE_BUFFER_LENGTH];
} watch_entry_t;

typedef struct {
    int inotify_fd;
    watch_entry_t watches[MAX_WATCHES];
    int count;
} watch_table_t;

typedef enum {
    CHANGE_ADDED,
    CHANGE_REMOVED,
    CHANGE_MODIFIED,
} change_type_t;

typedef struct {
    change_type_t type;
    char path[RESPONSE_BUFFER_LENGTH];
} change_t;

typedef struct {
    change_t entries[MAX_FILES];
    int count;
} change_list_t;


void * watch_directory(void * arg);

#endif //ROOTKIT_WATCHER_H
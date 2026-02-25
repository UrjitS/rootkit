#include "keylogger.h"
#include <stdio.h>
#include "utils.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <dirent.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>

uint32_t last_scan_code = 0;
int last_scan_code_valid = 0;
int last_scan_consumed = 0;
struct timeval start_time = {0, 0};
int start_time_valid = 0;
int printed_since_syn = 0;
static modifier_state_t modifiers = {0, 0, 0, 0, 0};


/**
 * Fix Parallels virtual keyboard scan code bugs
 * Only fix when we have a valid scan code AND the key code looks wrong
 */
int fix_parallels_key_code(const int code, const uint32_t scan_code, int *was_fixed) {
    if (was_fixed) {
        *was_fixed = 0;
    }

    if (scan_code == 0) {
        return code;  // No scan code available
    }

    // Parallels bug: HID usage for left shift but kernel reports wrong key
    if (scan_code == HID_USAGE_KEYBOARD_LEFTSHIFT &&
        code != KEY_LEFTSHIFT && code != KEY_RIGHTSHIFT) {
        if (was_fixed) {
            *was_fixed = 1;
        }
        return KEY_LEFTSHIFT;
    }

    // Parallels bug: HID usage for right shift but kernel reports wrong key
    if (scan_code == HID_USAGE_KEYBOARD_RIGHTSHIFT &&
        code != KEY_LEFTSHIFT && code != KEY_RIGHTSHIFT) {
        if (was_fixed) {
            *was_fixed = 1;
        }
        return KEY_RIGHTSHIFT;
    }

    return code;
}

/**
 * Update modifier key states
 */
void update_modifiers(const int code, const int value) {
    const int pressed = (value == 1);

    switch(code) {
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            modifiers.shift = pressed;
            break;
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            modifiers.ctrl = pressed;
            break;
        case KEY_LEFTALT:
        case KEY_RIGHTALT:
            modifiers.alt = pressed;
            break;
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA:
            modifiers.meta = pressed;
            break;
        case KEY_CAPSLOCK:
            // Capslock is a toggle - only change on press
            if (value == 1) {
                modifiers.capslock = !modifiers.capslock;
            }
            break;
        default: fprintf(stderr, "Hit Default Case in update_modifiers");
    }
}

/**
 * Print current modifier state
 */
void print_modifiers(void) {
    printf(" [");
    if (modifiers.shift) printf("SHIFT ");
    if (modifiers.ctrl) printf("CTRL ");
    if (modifiers.alt) printf("ALT ");
    if (modifiers.meta) printf("META ");
    if (modifiers.capslock) printf("CAPS ");
    if (!modifiers.shift && !modifiers.ctrl && !modifiers.alt &&
        !modifiers.meta && !modifiers.capslock) {
        printf("none");
    }
    printf("]");
}

/**
 * Print time relative to first event
 */
void print_relative_time(const struct timeval *ev_time) {
    char time_buf[32];

    if (!start_time_valid) {
        start_time = *ev_time;
        start_time_valid = 1;
        snprintf(time_buf, sizeof(time_buf), "+0.000000");
    } else {
        long sec_delta = ev_time->tv_sec - start_time.tv_sec;
        long usec_delta = ev_time->tv_usec - start_time.tv_usec;
        if (usec_delta < 0) {
            sec_delta--;
            usec_delta += 1000000;
        }
        snprintf(time_buf, sizeof(time_buf), "+%ld.%06ld", sec_delta, usec_delta);
    }

    printf("%-15s", time_buf);
}

/**
 * Convert event type to string
 * Note: Returns pointer to static buffer for unknown types - don't call multiple times in same printf
 */
const char* event_type_to_string(const int type) {
    switch(type) {
        case EV_SYN: return "EV_SYN";
        case EV_KEY: return "EV_KEY";
        case EV_REL: return "EV_REL";
        case EV_ABS: return "EV_ABS";
        case EV_MSC: return "EV_MSC";
        case EV_SW: return "EV_SW";
        case EV_LED: return "EV_LED";
        case EV_SND: return "EV_SND";
        case EV_REP: return "EV_REP";
        default: {
            static char buf[32];
            snprintf(buf, sizeof(buf), "TYPE_%d", type);
            return buf;
        }
    }
}

/**
 * Convert code to string (writes to provided buffer)
 */
const char* code_to_string_buf(const int type, const int code, char *buf, const size_t buflen) {
    if (type == EV_SYN) {
        switch(code) {
            case SYN_REPORT: return "SYN_REPORT";
            case SYN_CONFIG: return "SYN_CONFIG";
            case SYN_MT_REPORT: return "SYN_MT_REPORT";
            case SYN_DROPPED: return "SYN_DROPPED";
            default:
                snprintf(buf, buflen, "SYN_%d", code);
                return buf;
        }
    }

    if (type == EV_MSC) {
        switch(code) {
            case MSC_SCAN: return "MSC_SCAN";
            case MSC_SERIAL: return "MSC_SERIAL";
            case MSC_PULSELED: return "MSC_PULSELED";
            case MSC_GESTURE: return "MSC_GESTURE";
            case MSC_RAW: return "MSC_RAW";
            case MSC_TIMESTAMP: return "MSC_TIMESTAMP";
            default:
                snprintf(buf, buflen, "MSC_%d", code);
                return buf;
        }
    }

    if (type == EV_KEY) {
        // CHECK SPECIAL KEYS FIRST
        switch(code) {
            case KEY_ESC: return "KEY_ESC";
            case KEY_ENTER: return "KEY_ENTER";
            case KEY_BACKSPACE: return "KEY_BACKSPACE";
            case KEY_TAB: return "KEY_TAB";
            case KEY_SPACE: return "KEY_SPACE";
            case KEY_MINUS: return "KEY_MINUS";
            case KEY_EQUAL: return "KEY_EQUAL";
            case KEY_LEFTBRACE: return "KEY_LEFTBRACE";
            case KEY_RIGHTBRACE: return "KEY_RIGHTBRACE";
            case KEY_SEMICOLON: return "KEY_SEMICOLON";
            case KEY_APOSTROPHE: return "KEY_APOSTROPHE";
            case KEY_GRAVE: return "KEY_GRAVE";
            case KEY_BACKSLASH: return "KEY_BACKSLASH";
            case KEY_COMMA: return "KEY_COMMA";
            case KEY_DOT: return "KEY_DOT";
            case KEY_SLASH: return "KEY_SLASH";
            case KEY_CAPSLOCK: return "KEY_CAPSLOCK";
            case KEY_LEFTSHIFT: return "KEY_LEFTSHIFT";
            case KEY_RIGHTSHIFT: return "KEY_RIGHTSHIFT";
            case KEY_LEFTCTRL: return "KEY_LEFTCTRL";
            case KEY_RIGHTCTRL: return "KEY_RIGHTCTRL";
            case KEY_LEFTALT: return "KEY_LEFTALT";
            case KEY_RIGHTALT: return "KEY_RIGHTALT";
            case KEY_LEFTMETA: return "KEY_LEFTMETA";
            case KEY_RIGHTMETA: return "KEY_RIGHTMETA";
            case KEY_UP: return "KEY_UP";
            case KEY_DOWN: return "KEY_DOWN";
            case KEY_LEFT: return "KEY_LEFT";
            case KEY_RIGHT: return "KEY_RIGHT";
            case KEY_PAGEUP: return "KEY_PAGEUP";
            case KEY_PAGEDOWN: return "KEY_PAGEDOWN";
            case KEY_HOME: return "KEY_HOME";
            case KEY_END: return "KEY_END";
            case KEY_INSERT: return "KEY_INSERT";
            case KEY_DELETE: return "KEY_DELETE";
            default: fprintf(stderr, "Hit Default Case in code_to_string_buf");
        }

        // Function keys
        if (code >= KEY_F1 && code <= KEY_F12) {
            snprintf(buf, buflen, "KEY_F%d", code - KEY_F1 + 1);
            return buf;
        }

        // Letters
        if (code >= KEY_A && code <= KEY_Z) {
            snprintf(buf, buflen, "KEY_%c", 'A' + (code - KEY_A));
            return buf;
        }

        // Numbers
        if (code >= KEY_1 && code <= KEY_9) {
            snprintf(buf, buflen, "KEY_%d", code - KEY_1 + 1);
            return buf;
        }
        if (code == KEY_0) return "KEY_0";

        snprintf(buf, buflen, "KEY_%d", code);
        return buf;
    }

    snprintf(buf, buflen, "CODE_%d", code);
    return buf;
}

/**
 * Convert code to string (uses static buffer - not safe for multiple calls in same printf)
 */
const char* code_to_string(const int type, const int code) {
    static char buf[32];
    return code_to_string_buf(type, code, buf, sizeof(buf));
}

/**
 * Convert value to string based on event type and code
 */
const char* value_to_string(const int type, const int code, const int value) {
    static char buf[32];

    if (type == EV_KEY) {
        switch(value) {
            case 0: return "RELEASE";
            case 1: return "PRESS";
            case 2: return "REPEAT";
            default:
                snprintf(buf, sizeof(buf), "STATE_%d", value);
                return buf;
        }
    }

    if (type == EV_SYN) {
        snprintf(buf, sizeof(buf), "%d", value);
        return buf;
    }

    if (type == EV_MSC) {
        // MSC_SCAN is typically a scan code - show as hex
        // Other MSC types are various integers - show as decimal
        if (code == MSC_SCAN) {
            snprintf(buf, sizeof(buf), "0x%08x", (uint32_t)value);
        } else {
            snprintf(buf, sizeof(buf), "%d", value);
        }
        return buf;
    }

    snprintf(buf, sizeof(buf), "%d", value);
    return buf;
}

/**
 * Verify device supports keyboard events and looks like a keyboard
 */
int verify_device(const int fd) {
    unsigned long evbit[NBITS(EV_MAX + 1)];
    unsigned long keybit[NBITS(KEY_MAX + 1)];

    // Check event types
    memset(evbit, 0, sizeof(evbit));
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) {
        perror("ioctl EVIOCGBIT(0)");
        return 0;
    }

    if (!test_bit(EV_KEY, evbit)) {
        fprintf(stderr, "Error: Device does not support keyboard events (EV_KEY)\n");
        return 0;
    }

    // Check if it looks like a keyboard (not just a button/mouse)
    memset(keybit, 0, sizeof(keybit));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) {
        perror("ioctl EVIOCGBIT(EV_KEY)");
        return 0;
    }

    // A real keyboard should have some common keys
    if (!test_bit(KEY_A, keybit) && !test_bit(KEY_Q, keybit) &&
        !test_bit(KEY_ENTER, keybit) && !test_bit(KEY_SPACE, keybit) &&
        !test_bit(KEY_LEFTSHIFT, keybit)) {
        fprintf(stderr, "Warning: Device has EV_KEY but doesn't look like a keyboard\n");
        fprintf(stderr, "         (no common keyboard keys found)\n");
        fprintf(stderr, "         Continuing anyway...\n\n");
    }

    return 1;
}

/**
 * Print device information
 */
// void print_device_info(int fd) {
//     char name[256] = "Unknown";
//     char phys[256];
//     struct input_id id;
//     int has_name = 0;
//     int has_phys = 0;
//     int has_id = 0;
//
//     if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
//         printf("Device name: %s\n", name);
//         has_name = 1;
//     }
//
//     // Guard against empty string from EVIOCGPHYS
//     if (ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys) >= 0 && phys[0] != '\0') {
//         printf("Physical path: %s\n", phys);
//         has_phys = 1;
//     }
//
//     if (ioctl(fd, EVIOCGID, &id) >= 0) {
//         printf("Device ID: bus=0x%04x vendor=0x%04x product=0x%04x version=0x%04x\n",
//                id.bustype, id.vendor, id.product, id.version);
//         has_id = 1;
//     }
//
//     // Only mention missing info if we at least got the name
//     if (has_name && !has_phys && !has_id) {
//         printf("(No physical path or device ID available)\n");
//     }
// }

/**
 * Capture and display key events from device
 */
void capture_keys(const char *device_path) {
    struct input_event ev;
    fd_set readfds;
    struct timeval tv;
    int was_fixed;

    const int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        perror("Error opening device");
        fprintf(stderr, "Hint: Try running with sudo\n");
        return;
    }

    if (!verify_device(fd)) {
        close(fd);
        return;
    }

    printf("Capturing key events from: %s\n", device_path);
    printf("==================================================================================\n");
    printf("%-15s %-10s %-20s %-15s %-30s\n",
           "RelTime", "Type", "Code", "Value", "Modifiers");
    printf("==================================================================================\n");

    while (!exit_flag) {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms for responsive Ctrl+C

        const int ret = select(fd + 1, &readfds, NULL, NULL, &tv);

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select error");
            break;
        }
        if (ret == 0) {
            continue;
        }

        const ssize_t n = read(fd, &ev, sizeof(ev));

        // Handle EOF
        if (n == 0) {
            fprintf(stderr, "Device closed (EOF)\n");
            break;
        }

        // Handle short reads
        if (n != sizeof(ev)) {
            if (n < 0) {
                perror("read error");
                break;
            }
            fprintf(stderr, "Warning: Short read (%zd bytes, expected %zu)\n",
                    n, sizeof(ev));
            continue;
        }

        // Track scan codes from MSC_SCAN events (cast handles any signedness)
        if (ev.type == EV_MSC && ev.code == MSC_SCAN) {
            last_scan_code = (uint32_t)ev.value;
            last_scan_code_valid = 1;
            last_scan_consumed = 0;  // Fresh scan code, not yet used
        }

        if (ev.type == EV_KEY) {
            // Get scan code for fix only if valid and not already consumed
            const uint32_t scan_for_fix = (last_scan_code_valid && !last_scan_consumed) ? last_scan_code : 0;

            // Fix Parallels bugs
            const int corrected_code = fix_parallels_key_code(ev.code, scan_for_fix, &was_fixed);

            // Mark scan as consumed when we use it
            if (scan_for_fix != 0) {
                last_scan_consumed = 1;
            }

            print_relative_time(&ev.time);
            printf(" %-10s %-20s %-15s",
                   event_type_to_string(ev.type),
                   code_to_string(ev.type, corrected_code),
                   value_to_string(ev.type, ev.code, ev.value));

            // Print modifiers BEFORE updating (shows state at time of event)
            print_modifiers();

            // Show if we applied a fix (using separate buffers to avoid static buffer collision)
            if (was_fixed) {
                char raw_buf[32], fixed_buf[32];
                printf(" [FIXED: %s->%s]",
                       code_to_string_buf(ev.type, ev.code, raw_buf, sizeof(raw_buf)),
                       code_to_string_buf(ev.type, corrected_code, fixed_buf, sizeof(fixed_buf)));
            }

            printf("\n");
            fflush(stdout);

            // Update modifiers AFTER printing for next event
            update_modifiers(corrected_code, ev.value);

            printed_since_syn = 1;
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            // Only print event boundary if we've printed KEY events since last boundary
            if (printed_since_syn) {
                print_relative_time(&ev.time);
                printf(" %-10s %-20s %-15s [---event boundary---]\n",
                       event_type_to_string(ev.type),
                       code_to_string(ev.type, ev.code),
                       value_to_string(ev.type, ev.code, ev.value));
                fflush(stdout);
                printed_since_syn = 0;
            }
        } else {
            // Other events (MSC, LED, etc.) - don't trigger boundary markers
            print_relative_time(&ev.time);
            printf(" %-10s %-20s %-15s\n",
                   event_type_to_string(ev.type),
                   code_to_string(ev.type, ev.code),
                   value_to_string(ev.type, ev.code, ev.value));
            fflush(stdout);
            // Don't set printed_since_syn for non-KEY events
        }
    }

    close(fd);
    printf("\nCapture stopped.\n");
}



/**
 * Discover all keyboard devices in /dev/input
 */
void discover_keyboards(void) {
    struct dirent *entry;
    char path[PATH_MAX_LEN];
    int keyboard_count = 0;

    printf("Linux Keyboard Device Discovery\n");
    printf("================================\n\n");

    // Open /dev/input directory
    DIR *dir = opendir("/dev/input");
    if (!dir) {
        fprintf(stderr, "Error: Cannot open /dev/input: %s\n", strerror(errno));
        fprintf(stderr, "Hint: You may need to run this program with sudo\n");
        return;
    }

    printf("Scanning /dev/input for keyboard devices...\n\n");

    // Iterate through all entries
    while ((entry = readdir(dir)) != NULL) {
        // Only check event devices
        if (strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        // Build full path
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

        // Try to open device
        const int fd = open(path, O_RDONLY);
        if (fd < 0) {
            // Skip devices we can't access
            continue;
        }

        // Check if it's a keyboard
        if (is_keyboard(fd)) {
            keyboard_count++;
            printf("Keyboard #%d:\n", keyboard_count);
            print_device_info(path, fd);
            printf("\n");
        }

        close(fd);
    }

    closedir(dir);

    // Summary
    printf("================================\n");
    printf("Total keyboards found: %d\n", keyboard_count);

    if (keyboard_count == 0) {
        printf("\nNo keyboards detected. Possible reasons:\n");
        printf("  - Insufficient permissions (try running with sudo)\n");
        printf("  - No physical keyboard connected\n");
        printf("  - Keyboard drivers not loaded\n");
    } else {
        printf("\nNote: Multiple interfaces from the same physical device may be listed.\n");
        printf("This is normal for devices with separate standard/multimedia interfaces.\n");
    }
}

/**
 * Check if a device is a keyboard by examining its capabilities
 * Returns 1 if device is a keyboard, 0 otherwise
 */
int is_keyboard(const int fd) {
    unsigned long evbit[NBITS(EV_MAX)];
    unsigned long keybit[NBITS(KEY_MAX)];
    int keyboard_keys = 0;

    // Get supported event types
    memset(evbit, 0, sizeof(evbit));
    if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evbit) < 0) {
        return 0;
    }

    // Must support key events
    if (!test_bit(EV_KEY, evbit)) {
        return 0;
    }

    // Get supported keys
    memset(keybit, 0, sizeof(keybit));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), keybit) < 0) {
        return 0;
    }

    // Check for typical keyboard keys
    // A real keyboard should have alphanumeric keys

    // Check for letter keys (Q-P row as a sample)
    for (int i = KEY_Q; i <= KEY_P; i++) {
        if (test_bit(i, keybit)) {
            keyboard_keys++;
        }
    }

    // Also check for common keys
    if (test_bit(KEY_ENTER, keybit)) keyboard_keys++;
    if (test_bit(KEY_SPACE, keybit)) keyboard_keys++;

    // If we found several keyboard keys, it's likely a keyboard
    return keyboard_keys > 5;
}

/**
 * Get detailed device information
 */
void print_device_info(const char *path, const int fd) {
    char name[256] = "Unknown";
    char phys[256] = "Unknown";
    char uniq[256] = "Unknown";
    struct input_id device_info;
    unsigned long evbit[NBITS(EV_MAX)];

    // Get device name
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);

    // Get physical location
    if (ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys) < 0) {
        strcpy(phys, "N/A");
    }

    // Get unique identifier
    if (ioctl(fd, EVIOCGUNIQ(sizeof(uniq)), uniq) < 0) {
        strcpy(uniq, "N/A");
    }

    // Get device ID (vendor, product, etc.)
    if (ioctl(fd, EVIOCGID, &device_info) >= 0) {
        printf("  Device:   %s\n", path);
        printf("  Name:     %s\n", name);
        printf("  Physical: %s\n", phys);
        printf("  Unique:   %s\n", uniq);
        printf("  Vendor:   0x%04x\n", device_info.vendor);
        printf("  Product:  0x%04x\n", device_info.product);
        printf("  Version:  0x%04x\n", device_info.version);
        printf("  Bus Type: %d\n", device_info.bustype);
    } else {
        printf("  Device: %s\n", path);
        printf("  Name:   %s\n", name);
    }

    // Show supported event types
    memset(evbit, 0, sizeof(evbit));
    if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evbit) >= 0) {
        printf("  Events:   ");
        if (test_bit(EV_KEY, evbit)) printf("KEY ");
        if (test_bit(EV_REL, evbit)) printf("REL ");
        if (test_bit(EV_ABS, evbit)) printf("ABS ");
        if (test_bit(EV_LED, evbit)) printf("LED ");
        if (test_bit(EV_SND, evbit)) printf("SND ");
        printf("\n");
    }
}

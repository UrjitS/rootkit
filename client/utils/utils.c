#include "utils.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


_Atomic int exit_flag = false;

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
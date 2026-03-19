#include "utils/parse.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int util_parse_int_in_range(const char* text, int min, int max, int* out) {
    char* end;
    long  value;

    if(text == NULL || out == NULL || min > max) {
        return -1;
    }

    errno = 0;
    value = strtol(text, &end, 10);
    if(errno != 0 || *text == '\0' || *end != '\0') {
        return -1;
    }

    if(value < (long)min || value > (long)max) {
        return -1;
    }

    *out = (int)value;
    return 0;
}

int util_parse_positive_ll(const char* text, long long* out) {
    char*     end;
    long long value;

    if(text == NULL || out == NULL) {
        return -1;
    }

    errno = 0;
    value = strtoll(text, &end, 10);
    if(errno != 0 || *text == '\0' || *end != '\0' || value <= 0) {
        return -1;
    }

    *out = value;
    return 0;
}

int util_parse_bool(const char* text, int* out) {
    if(text == NULL || out == NULL) {
        return -1;
    }

    if(strcasecmp(text, "1") == 0 || strcasecmp(text, "true") == 0 ||
       strcasecmp(text, "yes") == 0 || strcasecmp(text, "on") == 0) {
        *out = 1;
        return 0;
    }

    if(strcasecmp(text, "0") == 0 || strcasecmp(text, "false") == 0 ||
       strcasecmp(text, "no") == 0 || strcasecmp(text, "off") == 0) {
        *out = 0;
        return 0;
    }

    return -1;
}

int util_copy_string(char* dst, size_t dstSize, const char* src) {
    size_t len;

    if(dst == NULL || src == NULL || dstSize == 0) {
        return -1;
    }

    len = strlen(src);
    if(len >= dstSize) {
        return -1;
    }

    memcpy(dst, src, len + 1);
    return 0;
}

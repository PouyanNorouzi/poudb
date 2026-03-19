#ifndef UTILS_PARSE_H
#define UTILS_PARSE_H

#include <stddef.h>

int util_parse_int_in_range(const char* text, int min, int max, int* out);
int util_parse_positive_ll(const char* text, long long* out);
int util_parse_bool(const char* text, int* out);
int util_copy_string(char* dst, size_t dstSize, const char* src);

#endif /* UTILS_PARSE_H */

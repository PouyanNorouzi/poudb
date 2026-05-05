#include "utils/log.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static LogLevel g_log_level = LOG_INFO;

static const char *level_tag(LogLevel level) {
    switch(level) {
        case LOG_ERROR:  return "ERROR";
        case LOG_WARN:   return "WARN ";
        case LOG_INFO:   return "INFO ";
        case LOG_DEBUG:  return "DEBUG";
        default:         return "?????";
    }
}

static void log_write(LogLevel level, const char *fmt, va_list ap) {
    if(level > g_log_level) {
        return;
    }

    char timebuf[20]; /* "YYYY-MM-DD HH:MM:SS" */
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_info);

    fprintf(stderr, "[%s] %s ", level_tag(level), timebuf);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

void log_init(LogLevel level) {
    g_log_level = level;
}

void log_set_level(LogLevel level) {
    g_log_level = level;
}

LogLevel log_level_from_string(const char *s) {
    if(strcasecmp(s, "silent") == 0) return LOG_SILENT;
    if(strcasecmp(s, "error")  == 0) return LOG_ERROR;
    if(strcasecmp(s, "warn")   == 0) return LOG_WARN;
    if(strcasecmp(s, "info")   == 0) return LOG_INFO;
    if(strcasecmp(s, "debug")  == 0) return LOG_DEBUG;
    return LOG_INFO;
}

const char* log_level_to_string(LogLevel level) {
    switch(level) {
        case LOG_SILENT: return "silent";
        case LOG_ERROR:  return "error";
        case LOG_WARN:   return "warn";
        case LOG_INFO:   return "info";
        case LOG_DEBUG:  return "debug";
        default:         return "unknown";
    }
}

void log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_write(LOG_ERROR, fmt, ap);
    va_end(ap);
}

void log_warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_write(LOG_WARN, fmt, ap);
    va_end(ap);
}

void log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_write(LOG_INFO, fmt, ap);
    va_end(ap);
}

void log_debug(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_write(LOG_DEBUG, fmt, ap);
    va_end(ap);
}

void log_errno(const char *msg) {
    log_error("%s: %s", msg, strerror(errno));
}

#ifndef UTILS_LOG_H
#define UTILS_LOG_H

typedef enum {
    LOG_SILENT = 0,
    LOG_ERROR  = 1,
    LOG_WARN   = 2,
    LOG_INFO   = 3,
    LOG_DEBUG  = 4
} LogLevel;

/**
 * Initialise the logger with the given level.
 * Should be called once after config is loaded.
 */
void log_init(LogLevel level);

/**
 * Update the active log level at runtime.
 */
void log_set_level(LogLevel level);

/**
 * Parse a log level from a string.
 * Accepts (case-insensitive): "silent", "error", "warn", "info", "debug".
 * Returns LOG_INFO for unknown strings.
 */
LogLevel log_level_from_string(const char *s);

/**
 * Convert a LogLevel back to a human-readable string ("silent", "error", etc.).
 */
const char* log_level_to_string(LogLevel level);

/**
 * Emit a message at ERROR level (always visible unless LOG_SILENT).
 */
void log_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * Emit a message at WARN level.
 */
void log_warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * Emit a message at INFO level.
 */
void log_info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * Emit a message at DEBUG level.
 */
void log_debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * Emit an ERROR message that appends ": <strerror(errno)>" to msg.
 * Equivalent to the old perror() calls.
 */
void log_errno(const char *msg);

#endif /* UTILS_LOG_H */

/**
 * Implementation of VSort logger system
 */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "vsort_logger.h"

// Default log level
static vsort_log_level_t current_log_level = VSORT_LOG_WARNING;

// Log level names for output
static const char *log_level_names[] = {
    "NONE",
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG"};

void vsort_log_init(vsort_log_level_t level)
{
    current_log_level = level;
}

void vsort_log_set_level(vsort_log_level_t level)
{
    current_log_level = level;
}

vsort_log_level_t vsort_log_get_level(void)
{
    return current_log_level;
}

void vsort_log(vsort_log_level_t level, const char *format, va_list args)
{
    if (level == VSORT_LOG_NONE || level > current_log_level)
        return;

    // Get current time
    time_t now;
    char time_str[20];
    time(&now);
    struct tm *tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // Print log header with timestamp and level
    fprintf(stderr, "[%s] [%s] ", time_str, log_level_names[level]);

    // Print message
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

void vsort_log_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsort_log(VSORT_LOG_ERROR, format, args);
    va_end(args);
}

void vsort_log_warning(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsort_log(VSORT_LOG_WARNING, format, args);
    va_end(args);
}

void vsort_log_info(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsort_log(VSORT_LOG_INFO, format, args);
    va_end(args);
}

void vsort_log_debug(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsort_log(VSORT_LOG_DEBUG, format, args);
    va_end(args);
}

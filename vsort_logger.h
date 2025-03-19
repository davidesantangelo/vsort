/**
 * Logger system for VSort library
 *
 * Provides configurable verbosity levels for logging instead of printf statements.
 * 
 * @author Davide Santangelo <https://github.com/davidesantangelo>
 * @license MIT
 */

#ifndef VSORT_LOGGER_H
#define VSORT_LOGGER_H

#include <stdarg.h>
#include "vsort.h"

#ifdef __cplusplus
extern "C" {
#endif

// Log levels
typedef enum {
    VSORT_LOG_NONE = 0,   // No logging
    VSORT_LOG_ERROR,      // Critical errors
    VSORT_LOG_WARNING,    // Warning messages
    VSORT_LOG_INFO,       // Informational messages
    VSORT_LOG_DEBUG       // Debug information
} vsort_log_level_t;

// Initialize logger system
VSORT_API void vsort_log_init(vsort_log_level_t level);

// Set log level
VSORT_API void vsort_log_set_level(vsort_log_level_t level);

// Get current log level
VSORT_API vsort_log_level_t vsort_log_get_level(void);

// Log functions for different levels
VSORT_API void vsort_log_error(const char *format, ...);
VSORT_API void vsort_log_warning(const char *format, ...);
VSORT_API void vsort_log_info(const char *format, ...);
VSORT_API void vsort_log_debug(const char *format, ...);

// Internal logging function
void vsort_log(vsort_log_level_t level, const char *format, va_list args);

#ifdef __cplusplus
}
#endif

#endif /* VSORT_LOGGER_H */

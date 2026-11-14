#ifndef VANILLABTN_HOST_DEBUG_H
#define VANILLABTN_HOST_DEBUG_H

#include <stdbool.h>

#ifdef _DEBUG

/* Debug-only logger. A separate console process owns the visible window, so it
 * survives if the game exits through an unhandled exception. */
bool host_debug_start(void);
void host_debug_stop(int exit_code);
void host_debug_log(const char *level, const char *area, const char *format, ...);

#define HOST_DEBUG_INFO(area, ...) host_debug_log("INFO", area, __VA_ARGS__)
#define HOST_DEBUG_WARN(area, ...) host_debug_log("WARN", area, __VA_ARGS__)
#define HOST_DEBUG_ERROR(area, ...) host_debug_log("ERROR", area, __VA_ARGS__)

#else

#define host_debug_start() false
#define host_debug_stop(exit_code) ((void)(exit_code))
#define HOST_DEBUG_INFO(area, ...) ((void)0)
#define HOST_DEBUG_WARN(area, ...) ((void)0)
#define HOST_DEBUG_ERROR(area, ...) ((void)0)

#endif

#endif

#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <stddef.h>
#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize NTP time synchronization
void time_sync_init(void);

// Check if time is synchronized
bool time_sync_is_ready(void);

// Get current time as string (HH:MM format)
void time_sync_get_time_string(char* buffer, size_t buffer_size);

// Get current time as struct tm
void time_sync_get_time(struct tm* timeinfo);

#ifdef __cplusplus
}
#endif

#endif // TIME_SYNC_H 
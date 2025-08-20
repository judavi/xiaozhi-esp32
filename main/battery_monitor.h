#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize battery monitoring
void battery_monitor_init(void);

// Get battery percentage (0-100)
int battery_monitor_get_percentage(void);

// Check if battery is charging
bool battery_monitor_is_charging(void);

// Get battery voltage in mV
int battery_monitor_get_voltage_mv(void);

// Get JSON representation of battery status
char* battery_monitor_get_json(void);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_MONITOR_H
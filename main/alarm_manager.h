#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ALARM_NAME_LEN 32

typedef struct {
    bool enabled;
    int hour;           // 0-23
    int minute;         // 0-59
    bool days[7];       // Sunday=0, Monday=1, ..., Saturday=6
    char name[MAX_ALARM_NAME_LEN];
} alarm_config_t;

// Initialize alarm manager and load settings from NVS
void alarm_manager_init(void);

// Get alarm configuration (alarm_id: 0 or 1)
alarm_config_t* alarm_manager_get_alarm(int alarm_id);

// Set alarm configuration
void alarm_manager_set_alarm(int alarm_id, const alarm_config_t* alarm);

// Save all alarms to NVS
void alarm_manager_save(void);

// Check if any alarm should trigger now
bool alarm_manager_check_alarms(void);

// Get JSON representation of all alarms
char* alarm_manager_get_json(void);

// Set alarms from JSON string
bool alarm_manager_set_from_json(const char* json_str);

#ifdef __cplusplus
}
#endif

#endif // ALARM_MANAGER_H 
#include "alarm_manager.h"
#include "time_sync.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <cJSON.h>

static const char *TAG = "ALARM_MANAGER";
static const char *NVS_NAMESPACE = "alarms";

static alarm_config_t alarms[2];
static bool initialized = false;

void alarm_manager_init(void) {
    ESP_LOGI(TAG, "⏰ Initializing Alarm Manager...");
    
    // Initialize default alarms
    for (int i = 0; i < 2; i++) {
        alarms[i].enabled = false;
        alarms[i].hour = 7;
        alarms[i].minute = 0;
        snprintf(alarms[i].name, MAX_ALARM_NAME_LEN, "Alarm %d", i + 1);
        
        // Default: weekdays only
        for (int day = 0; day < 7; day++) {
            alarms[i].days[day] = (day >= 1 && day <= 5); // Monday to Friday
        }
    }
    
    // Load from NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        for (int i = 0; i < 2; i++) {
            char key[16];
            snprintf(key, sizeof(key), "alarm%d", i);
            
            size_t required_size = sizeof(alarm_config_t);
            err = nvs_get_blob(nvs_handle, key, &alarms[i], &required_size);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "✅ Loaded alarm %d: %s at %02d:%02d", i, alarms[i].name, alarms[i].hour, alarms[i].minute);
            } else {
                ESP_LOGI(TAG, "📝 Using default settings for alarm %d", i);
            }
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGW(TAG, "⚠️ Could not open NVS namespace, using defaults");
    }
    
    initialized = true;
    ESP_LOGI(TAG, "⏰ Alarm Manager initialized");
}

alarm_config_t* alarm_manager_get_alarm(int alarm_id) {
    if (alarm_id < 0 || alarm_id >= 2) return NULL;
    return &alarms[alarm_id];
}

void alarm_manager_set_alarm(int alarm_id, const alarm_config_t* alarm) {
    if (alarm_id < 0 || alarm_id >= 2 || !alarm) return;
    
    memcpy(&alarms[alarm_id], alarm, sizeof(alarm_config_t));
    ESP_LOGI(TAG, "📝 Updated alarm %d: %s at %02d:%02d", alarm_id, alarm->name, alarm->hour, alarm->minute);
}

void alarm_manager_save(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to open NVS for saving");
        return;
    }
    
    for (int i = 0; i < 2; i++) {
        char key[16];
        snprintf(key, sizeof(key), "alarm%d", i);
        
        err = nvs_set_blob(nvs_handle, key, &alarms[i], sizeof(alarm_config_t));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to save alarm %d", i);
        }
    }
    
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "💾 Alarms saved to NVS");
    } else {
        ESP_LOGE(TAG, "❌ Failed to commit NVS");
    }
}

bool alarm_manager_check_alarms(void) {
    if (!initialized || !time_sync_is_ready()) return false;
    
    struct tm timeinfo;
    time_sync_get_time(&timeinfo);
    
    for (int i = 0; i < 2; i++) {
        if (!alarms[i].enabled) continue;
        
        // Check if current day is enabled
        if (!alarms[i].days[timeinfo.tm_wday]) continue;
        
        // Check if time matches (only check hour and minute)
        if (timeinfo.tm_hour == alarms[i].hour && timeinfo.tm_min == alarms[i].minute) {
            ESP_LOGI(TAG, "🚨 ALARM TRIGGERED: %s", alarms[i].name);
            return true;
        }
    }
    
    return false;
}

char* alarm_manager_get_json(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON *alarms_array = cJSON_CreateArray();
    
    for (int i = 0; i < 2; i++) {
        cJSON *alarm_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(alarm_obj, "id", i);
        cJSON_AddBoolToObject(alarm_obj, "enabled", alarms[i].enabled);
        cJSON_AddNumberToObject(alarm_obj, "hour", alarms[i].hour);
        cJSON_AddNumberToObject(alarm_obj, "minute", alarms[i].minute);
        cJSON_AddStringToObject(alarm_obj, "name", alarms[i].name);
        
        cJSON *days_array = cJSON_CreateArray();
        for (int day = 0; day < 7; day++) {
            cJSON_AddItemToArray(days_array, cJSON_CreateBool(alarms[i].days[day]));
        }
        cJSON_AddItemToObject(alarm_obj, "days", days_array);
        
        cJSON_AddItemToArray(alarms_array, alarm_obj);
    }
    
    cJSON_AddItemToObject(root, "alarms", alarms_array);
    
    // Add current time
    char time_str[64];
    time_sync_get_time_string(time_str, sizeof(time_str));
    cJSON_AddStringToObject(root, "current_time", time_str);
    cJSON_AddBoolToObject(root, "time_synced", time_sync_is_ready());
    
    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);
    
    return json_string;
}

bool alarm_manager_set_from_json(const char* json_str) {
    cJSON *root = cJSON_Parse(json_str);
    if (!root) return false;
    
    cJSON *alarms_array = cJSON_GetObjectItem(root, "alarms");
    if (!cJSON_IsArray(alarms_array)) {
        cJSON_Delete(root);
        return false;
    }
    
    int array_size = cJSON_GetArraySize(alarms_array);
    for (int i = 0; i < array_size && i < 2; i++) {
        cJSON *alarm_obj = cJSON_GetArrayItem(alarms_array, i);
        if (!cJSON_IsObject(alarm_obj)) continue;
        
        cJSON *id = cJSON_GetObjectItem(alarm_obj, "id");
        if (!cJSON_IsNumber(id) || id->valueint < 0 || id->valueint >= 2) continue;
        
        int alarm_id = id->valueint;
        
        cJSON *enabled = cJSON_GetObjectItem(alarm_obj, "enabled");
        if (cJSON_IsBool(enabled)) {
            alarms[alarm_id].enabled = cJSON_IsTrue(enabled);
        }
        
        cJSON *hour = cJSON_GetObjectItem(alarm_obj, "hour");
        if (cJSON_IsNumber(hour) && hour->valueint >= 0 && hour->valueint <= 23) {
            alarms[alarm_id].hour = hour->valueint;
        }
        
        cJSON *minute = cJSON_GetObjectItem(alarm_obj, "minute");
        if (cJSON_IsNumber(minute) && minute->valueint >= 0 && minute->valueint <= 59) {
            alarms[alarm_id].minute = minute->valueint;
        }
        
        cJSON *name = cJSON_GetObjectItem(alarm_obj, "name");
        if (cJSON_IsString(name)) {
            strncpy(alarms[alarm_id].name, name->valuestring, MAX_ALARM_NAME_LEN - 1);
            alarms[alarm_id].name[MAX_ALARM_NAME_LEN - 1] = '\0';
        }
        
        cJSON *days_array = cJSON_GetObjectItem(alarm_obj, "days");
        if (cJSON_IsArray(days_array) && cJSON_GetArraySize(days_array) == 7) {
            for (int day = 0; day < 7; day++) {
                cJSON *day_enabled = cJSON_GetArrayItem(days_array, day);
                if (cJSON_IsBool(day_enabled)) {
                    alarms[alarm_id].days[day] = cJSON_IsTrue(day_enabled);
                }
            }
        }
    }
    
    cJSON_Delete(root);
    
    // Save to NVS
    alarm_manager_save();
    
    ESP_LOGI(TAG, "📝 Alarms updated from JSON");
    return true;
} 
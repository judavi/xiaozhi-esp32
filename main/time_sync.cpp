#include "time_sync.h"
#include <esp_log.h>
#include <esp_sntp.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

static const char *TAG = "TIME_SYNC";

static bool time_synced = false;

// SNTP sync notification callback
static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "🕐 Time synchronized from NTP server!");
    time_synced = true;
    
    // Print current time
    char time_str[64];
    time_sync_get_time_string(time_str, sizeof(time_str));
    ESP_LOGI(TAG, "⏰ Current time: %s", time_str);
}

void time_sync_init(void) {
    ESP_LOGI(TAG, "🌐 Initializing NTP time synchronization...");
    
    // Set timezone to your location (adjust as needed)
    setenv("TZ", "PST8PDT,M3.2.0/2,M11.1.0", 1);
    tzset();
    
    // Initialize SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
    
    ESP_LOGI(TAG, "📡 NTP sync started, waiting for time synchronization...");
}

bool time_sync_is_ready(void) {
    return time_synced;
}

void time_sync_get_time_string(char* buffer, size_t buffer_size) {
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Format as "Monday, Jan 15, 2024 - 14:30"
    strftime(buffer, buffer_size, "%A, %b %d, %Y - %H:%M", &timeinfo);
}

void time_sync_get_time(struct tm* timeinfo) {
    time_t now;
    time(&now);
    localtime_r(&now, timeinfo);
} 
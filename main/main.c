#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "simple_wifi.h"
#include "alarm_manager.h"
#include "time_sync.h"
#include "battery_monitor.h"
#include "sound_manager.h"

static const char *TAG = "BASICWIFI";

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "🚀 ESP32-S3 Basic WiFi Project Starting...");
    ESP_LOGI(TAG, "📡 Target Network: JupiHouse");
    ESP_LOGI(TAG, "🔐 Security: WPA2-PSK");

    // Initialize alarm manager
    alarm_manager_init();
    
    // Initialize battery monitor
    battery_monitor_init();
    
    // Initialize sound manager
    sound_manager_init();

    // Initialize and start WiFi connection
    ESP_LOGI(TAG, "🔄 Initializing WiFi connection...");
    initialize_simple_wifi();
    ESP_LOGI(TAG, "✨ WiFi initialization complete!");
    ESP_LOGI(TAG, "🎯 Watch for '🎉 WiFi connected successfully!' message");

    // Main loop - check for alarms every minute
    ESP_LOGI(TAG, "🔄 Main loop started - checking for alarms...");
    
    while (1) {
        // Check if any alarms should trigger
        if (alarm_manager_check_alarms()) {
            ESP_LOGI(TAG, "🚨 ALARM TRIGGERED! Playing alarm sound...");
            // Here you could add buzzer/speaker control
            // For now, just log the alarm
        }
        
        // Check every 30 seconds to avoid missing alarms
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
} 
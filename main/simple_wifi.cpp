#include "simple_wifi.h"
#include "time_sync.h"
#include "http_server.h"
#include "alarm_manager.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <string.h>

static const char *TAG = "SIMPLE_WIFI";

// WiFi credentials
static const char* WIFI_SSID = "JupiHouse";
static const char* WIFI_PASSWORD = "Cosmo2024";

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "📡 WiFi station started");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "🔄 WiFi disconnected, trying to reconnect...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "🎉 WiFi connected successfully! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "🌐 Connected to SSID: %s", WIFI_SSID);
        ESP_LOGI(TAG, "🔐 Authentication: WPA2-PSK");
        ESP_LOGI(TAG, "✅ Internet connectivity: Available");
        
        // Start time synchronization
        ESP_LOGI(TAG, "🕐 Starting NTP time synchronization...");
        time_sync_init();
        
        // Start HTTP server for alarm configuration
        ESP_LOGI(TAG, "🌐 Starting HTTP server...");
        http_server_start();
        
        ESP_LOGI(TAG, "🔗 Alarm configuration available at: http://" IPSTR "/", IP2STR(&event->ip_info.ip));
    }
}

extern "C" {
    void initialize_simple_wifi();
}

void initialize_simple_wifi() {
    ESP_LOGI(TAG, "🚀 Initializing Simple WiFi...");
    
    // Initialize WiFi
    esp_netif_init();
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register WiFi event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    // Configure WiFi connection
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASSWORD);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "📶 WiFi connection initiated to SSID: %s", WIFI_SSID);
} 
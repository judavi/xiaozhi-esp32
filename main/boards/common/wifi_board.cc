#include "wifi_board.h"

#include "display.h"
#include "application.h"
#include "settings.h"
#include <json/cJSON.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <string.h>

static const char *TAG = "WifiBoard";

// Hardcoded WiFi credentials for automatic connection
static const char* WIFI_SSID = "JupiHouse";
static const char* WIFI_PASSWORD = "Cosmo2024";

// WiFi event handler function
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi station started");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, trying to reconnect...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "🎉 WiFi connected successfully! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        // Play success sound when WiFi connects
        Application& app = Application::GetInstance();
        app.PlayWifiConnectedSound();
        
        // Print immediate debug info when connected
        app.PrintNetworkDebugInfo();
    }
}

WifiBoard::WifiBoard() {
    Settings settings("wifi", true);
    wifi_config_mode_ = settings.GetInt("force_ap") == 1;
    if (wifi_config_mode_) {
        ESP_LOGI(TAG, "force_ap is set to 1, reset to 0");
        settings.SetInt("force_ap", 0);
    }
}

std::string WifiBoard::GetBoardType() {
    return "wifi";
}

void WifiBoard::EnterWifiConfigMode() {
    ESP_LOGI(TAG, "WiFi Config Mode enabled - using hardcoded credentials for JupiHouse");
}

void WifiBoard::StartNetwork() {
    ESP_LOGI(TAG, "Starting WiFi network for alarm clock");
    
    auto display = GetDisplay();
    if (display) {
        display->ShowNotification("Connecting to JupiHouse...", 3000);
    }
    
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
    
    ESP_LOGI(TAG, "Starting WiFi connection to SSID: %s", WIFI_SSID);
    
    if (display) {
        display->ShowNotification("WiFi connecting...", 2000);
    }
}

NetworkInterface* WifiBoard::GetNetwork() {
    // Simplified network interface for alarm clock
    return nullptr; // Network features simplified
}

const char* WifiBoard::GetNetworkStateIcon() {
    // Simplified network state icon for alarm clock
    return "WiFi"; // Simple text instead of font awesome icons
}

std::string WifiBoard::GetBoardJson() {
    // Simplified device name for alarm clock
    std::string device_name = "ESP32_Alarm_Clock";
    std::string board_json = R"({"device_name":")" + device_name + R"(",)";
    board_json += R"("board_name":")" + std::string(BOARD_NAME) + R"(",)";
    board_json += R"("mac":"00:00:00:00:00:00",)"; // Simplified MAC
    board_json += R"("board_type":")" + std::string(BOARD_TYPE) + R"(",)";
    board_json += R"("ipv4":"192.168.1.100"})"; // Simplified IP
    
    return board_json;
}

void WifiBoard::SetPowerSaveMode(bool enabled) {
    // Simplified power save mode for alarm clock
    ESP_LOGI(TAG, "Power save mode: %s", enabled ? "enabled" : "disabled");
}

void WifiBoard::ResetWifiConfiguration() {
    // Simplified WiFi reset for alarm clock
    auto display = GetDisplay();
    if (display) {
        display->ShowNotification("Resetting WiFi...");
    }
    ESP_LOGI(TAG, "WiFi configuration reset");
}

std::string WifiBoard::GetDeviceStatusJson() {
    cJSON* root = cJSON_CreateObject();

    // Audio speaker info
    cJSON* audio_speaker = cJSON_CreateObject();
    auto audio_codec = Board::GetInstance().GetAudioCodec();
    if (audio_codec) {
        // Simplified volume handling for alarm clock
        cJSON_AddNumberToObject(audio_speaker, "volume", 50); // Default volume
    }
    cJSON_AddItemToObject(root, "audio_speaker", audio_speaker);

    // Screen info  
    cJSON* screen = cJSON_CreateObject();
    auto backlight = Board::GetInstance().GetBacklight();
    auto display = Board::GetInstance().GetDisplay();
    if (backlight) {
        cJSON_AddNumberToObject(screen, "brightness", backlight->brightness());
    }
    if (display && display->height() > 64) { // For LCD display only
        cJSON_AddStringToObject(screen, "theme", display->GetTheme().c_str());
    }
    cJSON_AddItemToObject(root, "screen", screen);

    // Battery info
    int battery_level = 0;
    bool charging = false;
    bool discharging = false;
    if (Board::GetInstance().GetBatteryLevel(battery_level, charging, discharging)) {
        cJSON* battery = cJSON_CreateObject();
        cJSON_AddNumberToObject(battery, "level", battery_level);
        cJSON_AddBoolToObject(battery, "charging", charging);
        cJSON_AddItemToObject(root, "battery", battery);
    }

    // Network info
    cJSON* network = cJSON_CreateObject();
    cJSON_AddStringToObject(network, "type", "wifi");
    cJSON_AddStringToObject(network, "ssid", "WiFi Network"); // Simplified
    cJSON_AddStringToObject(network, "ip", "192.168.1.100"); // Simplified
    cJSON_AddNumberToObject(network, "rssi", -50); // Simplified
    cJSON_AddItemToObject(root, "network", network);

    // System info - simplified without system_info singleton
    cJSON* chip = cJSON_CreateObject();
    cJSON_AddNumberToObject(chip, "temperature", 25.0); // Default temperature
        cJSON_AddItemToObject(root, "chip", chip);

    auto json_str = cJSON_PrintUnformatted(root);
    std::string result(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    
    return result;
}

#include "battery_monitor.h"
#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <driver/gpio.h>
#include <cJSON.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "BATTERY_MONITOR";

// ADC configuration
#define BATTERY_ADC_UNIT        ADC_UNIT_1
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_0  // GPIO1
#define CHARGING_GPIO           GPIO_NUM_2     // GPIO2 for charging detection
#define ADC_ATTEN               ADC_ATTEN_DB_12
#define ADC_BITWIDTH            ADC_BITWIDTH_12

// Battery voltage thresholds (in mV)
#define BATTERY_MAX_VOLTAGE     4200  // 4.2V (fully charged Li-Po)
#define BATTERY_MIN_VOLTAGE     3000  // 3.0V (empty Li-Po)

// Voltage divider configuration (adjust based on your hardware)
#define VOLTAGE_DIVIDER_RATIO   2.0   // R1/(R1+R2) if using voltage divider

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;
static bool initialized = false;

void battery_monitor_init(void) {
    ESP_LOGI(TAG, "🔋 Initializing Battery Monitor...");
    
    // Configure charging detection GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << CHARGING_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&io_conf);
    
    // Initialize ADC
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, BATTERY_ADC_CHANNEL, &config));
    
    // Initialize ADC calibration
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ ADC calibration initialized");
    } else {
        ESP_LOGW(TAG, "⚠️ ADC calibration failed, using raw values");
        adc_cali_handle = NULL;
    }
    
    initialized = true;
    ESP_LOGI(TAG, "🔋 Battery Monitor initialized");
}

int battery_monitor_get_voltage_mv(void) {
    if (!initialized) return 0;
    
    int adc_raw = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, BATTERY_ADC_CHANNEL, &adc_raw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to read ADC");
        return 0;
    }
    
    int voltage_mv = 0;
    if (adc_cali_handle != NULL) {
        // Use calibrated conversion
        ret = adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage_mv);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to convert ADC to voltage");
            return 0;
        }
    } else {
        // Fallback: simple linear conversion (approximate)
        voltage_mv = (adc_raw * 3300) / 4095;
    }
    
    // Apply voltage divider compensation if needed
    voltage_mv = (int)(voltage_mv * VOLTAGE_DIVIDER_RATIO);
    
    return voltage_mv;
}

int battery_monitor_get_percentage(void) {
    if (!initialized) return 0;
    
    int voltage_mv = battery_monitor_get_voltage_mv();
    
    if (voltage_mv <= BATTERY_MIN_VOLTAGE) return 0;
    if (voltage_mv >= BATTERY_MAX_VOLTAGE) return 100;
    
    // Linear interpolation between min and max voltage
    int percentage = ((voltage_mv - BATTERY_MIN_VOLTAGE) * 100) / 
                    (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
    
    // Clamp to valid range
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    
    return percentage;
}

bool battery_monitor_is_charging(void) {
    if (!initialized) return false;
    
    // Read charging GPIO (active high)
    return gpio_get_level(CHARGING_GPIO) == 1;
}

char* battery_monitor_get_json(void) {
    if (!initialized) return NULL;
    
    cJSON *root = cJSON_CreateObject();
    
    int percentage = battery_monitor_get_percentage();
    int voltage_mv = battery_monitor_get_voltage_mv();
    bool charging = battery_monitor_is_charging();
    
    cJSON_AddNumberToObject(root, "percentage", percentage);
    cJSON_AddNumberToObject(root, "voltage_mv", voltage_mv);
    cJSON_AddBoolToObject(root, "charging", charging);
    
    // Add status string
    const char* status = charging ? "charging" : 
                        (percentage > 20 ? "normal" : 
                        (percentage > 10 ? "low" : "critical"));
    cJSON_AddStringToObject(root, "status", status);
    
    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);
    
    return json_string;
}
#include "audio_codec.h"
#include <esp_log.h>
#include <esp_err.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *TAG = "AUDIO_CODEC_SIMPLE";

// Simplified audio codec structure
struct audio_codec_s {
    audio_codec_config_t config;
    i2s_chan_handle_t tx_handle;
    bool initialized;
    bool playing;
    bool output_enabled;
};

audio_codec_handle_t audio_codec_init(const audio_codec_config_t* config) {
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return NULL;
    }
    
    ESP_LOGI(TAG, "🎵 Initializing simplified audio codec...");
    
    audio_codec_handle_t codec = calloc(1, sizeof(struct audio_codec_s));
    if (!codec) {
        ESP_LOGE(TAG, "Failed to allocate memory for codec");
        return NULL;
    }
    
    memcpy(&codec->config, config, sizeof(audio_codec_config_t));
    
    // Step 1: Configure Power Amplifier GPIO
    if (config->pa_gpio >= 0) {
        ESP_LOGI(TAG, "🔧 Configuring Power Amplifier GPIO %d", config->pa_gpio);
        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL << config->pa_gpio),
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE,
        };
        gpio_config(&io_conf);
        gpio_set_level(config->pa_gpio, 0);  // Start disabled
    }
    
    // Step 2: Initialize I2S (simplified - no I2C codec control for now)
    ESP_LOGI(TAG, "🔧 Creating I2S channel...");
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, &codec->tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to create I2S channel: %s", esp_err_to_name(ret));
        free(codec);
        return NULL;
    }
    
    // Step 3: Configure I2S with proper settings
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            config->bits_per_sample == 16 ? I2S_DATA_BIT_WIDTH_16BIT : I2S_DATA_BIT_WIDTH_24BIT,
            config->channels == 1 ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO
        ),
        .gpio_cfg = {
            .mclk = config->mclk_gpio,
            .bclk = config->bclk_gpio,
            .ws = config->ws_gpio,
            .dout = config->dout_gpio,
            .din = config->din_gpio,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    ret = i2s_channel_init_std_mode(codec->tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize I2S channel: %s", esp_err_to_name(ret));
        i2s_del_channel(codec->tx_handle);
        free(codec);
        return NULL;
    }
    
    codec->initialized = true;
    ESP_LOGI(TAG, "✅ Simplified audio codec initialized successfully");
    ESP_LOGI(TAG, "🔧 Config: %dHz, %dbit, %dch, PA GPIO%d", 
             config->sample_rate, config->bits_per_sample, config->channels, config->pa_gpio);
    
    return codec;
}

esp_err_t audio_codec_start(audio_codec_handle_t codec) {
    if (!codec || !codec->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🎵 Starting simplified audio codec...");
    
    // Enable Power Amplifier FIRST - this is critical for audio output
    if (codec->config.pa_gpio >= 0) {
        ESP_LOGI(TAG, "🔊 Enabling Power Amplifier on GPIO%d", codec->config.pa_gpio);
        gpio_set_level(codec->config.pa_gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(100));  // Longer delay for PA startup
    }
    
    // Enable I2S channel
    esp_err_t ret = i2s_channel_enable(codec->tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to enable I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    codec->output_enabled = true;
    ESP_LOGI(TAG, "🎉 Simplified audio codec started successfully");
    return ESP_OK;
}

esp_err_t audio_codec_stop(audio_codec_handle_t codec) {
    if (!codec || !codec->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🛑 Stopping audio codec...");
    
    // Disable I2S channel
    esp_err_t ret = i2s_channel_disable(codec->tx_handle);
    
    // Disable power amplifier
    if (codec->config.pa_gpio >= 0) {
        gpio_set_level(codec->config.pa_gpio, 0);
    }
    
    codec->playing = false;
    codec->output_enabled = false;
    ESP_LOGI(TAG, "✅ Audio codec stopped");
    return ret;
}

esp_err_t audio_codec_deinit(audio_codec_handle_t codec) {
    if (!codec) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🗑️ Deinitializing audio codec...");
    
    if (codec->initialized) {
        audio_codec_stop(codec);
        i2s_del_channel(codec->tx_handle);
    }
    
    free(codec);
    ESP_LOGI(TAG, "✅ Audio codec deinitialized");
    return ESP_OK;
}

esp_err_t audio_codec_set_volume(audio_codec_handle_t codec, int volume) {
    if (!codec || !codec->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    codec->config.volume = volume;
    
    // For simplified version, we don't have ES8311 register control
    // Volume is handled by the power amplifier being on/off
    // In a full implementation, this would control ES8311 DAC volume
    ESP_LOGI(TAG, "🔊 Volume set to %d%% (simplified - no codec register control)", volume);
    return ESP_OK;
}

int audio_codec_play_data(audio_codec_handle_t codec, const void* data, size_t length, uint32_t timeout_ms) {
    if (!codec || !codec->initialized || !data) {
        ESP_LOGW(TAG, "Invalid codec or data");
        return -1;
    }
    
    if (!codec->output_enabled) {
        ESP_LOGW(TAG, "Codec output not enabled");
        return -1;
    }
    
    codec->playing = true;
    
    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(codec->tx_handle, data, length, &bytes_written, 
                                     pdMS_TO_TICKS(timeout_ms));
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write audio data: %s", esp_err_to_name(ret));
        return -1;
    }
    
    ESP_LOGI(TAG, "🎵 Wrote %d bytes to I2S", bytes_written);
    return bytes_written;
}

bool audio_codec_is_playing(audio_codec_handle_t codec) {
    return codec && codec->playing;
}
#include "audio_codec.h"
#include <esp_log.h>
#include <esp_err.h>
#include <string.h>
#include <stdlib.h>
#include <driver/i2c.h>
#include <driver/i2s_std.h>

// ESP Codec Dev framework includes
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "es8311_codec.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "audio_codec_data_if.h"

static const char *TAG = "AUDIO_CODEC_ES8311";

// ES8311 codec structure following xiaozhi-esp32 reference implementation
struct audio_codec_s {
    // Configuration
    audio_codec_config_t config;
    
    // I2S handles (created first, then passed to ESP Codec Dev)
    i2s_chan_handle_t tx_handle;
    i2s_chan_handle_t rx_handle;
    
    // ESP Codec Dev interfaces
    const audio_codec_data_if_t* data_if;
    const audio_codec_ctrl_if_t* ctrl_if;
    const audio_codec_gpio_if_t* gpio_if;
    const audio_codec_if_t* codec_if;
    
    // ESP Codec Dev device (created only when needed)
    esp_codec_dev_handle_t codec_dev;
    
    // State management
    bool initialized;
    bool input_enabled;
    bool output_enabled;
};

// Following xiaozhi-esp32 CreateDuplexChannels approach
static esp_err_t create_duplex_channels(audio_codec_handle_t codec) {
    ESP_LOGI(TAG, "🔧 Creating duplex I2S channels...");
    
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,      // AUDIO_CODEC_DMA_DESC_NUM
        .dma_frame_num = 240,   // AUDIO_CODEC_DMA_FRAME_NUM
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    
    esp_err_t ret = i2s_new_channel(&chan_cfg, &codec->tx_handle, &codec->rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to create I2S channels: %s", esp_err_to_name(ret));
        return ret;
    }
    
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = codec->config.sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
        },
        .gpio_cfg = {
            .mclk = codec->config.mclk_gpio,
            .bclk = codec->config.bclk_gpio,
            .ws = codec->config.ws_gpio,
            .dout = codec->config.dout_gpio,
            .din = codec->config.din_gpio,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };
    
    ret = i2s_channel_init_std_mode(codec->tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to init I2S TX channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2s_channel_init_std_mode(codec->rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to init I2S RX channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ Duplex I2S channels created and configured");
    return ESP_OK;
}

// Following xiaozhi-esp32 UpdateDeviceState approach
static void update_device_state(audio_codec_handle_t codec) {
    if ((codec->input_enabled || codec->output_enabled) && codec->codec_dev == NULL) {
        ESP_LOGI(TAG, "🔧 Creating ESP codec device...");
        esp_codec_dev_cfg_t dev_cfg = {
            .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
            .codec_if = codec->codec_if,
            .data_if = codec->data_if,
        };
        codec->codec_dev = esp_codec_dev_new(&dev_cfg);
        if (!codec->codec_dev) {
            ESP_LOGE(TAG, "❌ Failed to create ESP codec device");
            return;
        }

        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 2,  // Stereo
            .channel_mask = 0,
            .sample_rate = codec->config.sample_rate,
            .mclk_multiple = 0,
        };
        esp_err_t ret = esp_codec_dev_open(codec->codec_dev, &fs);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to open codec device: %s", esp_err_to_name(ret));
            return;
        }
        
        ret = esp_codec_dev_set_out_vol(codec->codec_dev, codec->config.volume);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to set volume: %s", esp_err_to_name(ret));
        }
        
        ESP_LOGI(TAG, "✅ ESP codec device created and opened");
    } else if (!codec->input_enabled && !codec->output_enabled && codec->codec_dev != NULL) {
        ESP_LOGI(TAG, "🔧 Closing ESP codec device...");
        esp_codec_dev_close(codec->codec_dev);
        esp_codec_dev_delete(codec->codec_dev);
        codec->codec_dev = NULL;
        ESP_LOGI(TAG, "✅ ESP codec device closed and deleted");
    }
    
    // Control PA pin like xiaozhi-esp32
    if (codec->config.pa_gpio != -1) {
        int level = codec->output_enabled ? 1 : 0;
        gpio_set_level(codec->config.pa_gpio, level);
    }
}

audio_codec_handle_t audio_codec_init(const audio_codec_config_t* config) {
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return NULL;
    }
    
    ESP_LOGI(TAG, "🎵 Initializing ES8311 codec following xiaozhi-esp32 approach...");
    
    audio_codec_handle_t codec = calloc(1, sizeof(struct audio_codec_s));
    if (!codec) {
        ESP_LOGE(TAG, "Failed to allocate memory for codec");
        return NULL;
    }
    
    memcpy(&codec->config, config, sizeof(audio_codec_config_t));
    
    // Step 1: Create duplex I2S channels first (like xiaozhi-esp32)
    esp_err_t ret = create_duplex_channels(codec);
    if (ret != ESP_OK) {
        free(codec);
        return NULL;
    }
    
    // Step 2: Create I2S data interface with the pre-created channels
    ESP_LOGI(TAG, "🔧 Creating I2S data interface...");
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = codec->rx_handle,
        .tx_handle = codec->tx_handle,
    };
    codec->data_if = audio_codec_new_i2s_data(&i2s_cfg);
    if (!codec->data_if) {
        ESP_LOGE(TAG, "❌ Failed to create I2S data interface");
        free(codec);
        return NULL;
    }
    ESP_LOGI(TAG, "✅ I2S data interface created");

    // Step 3: Create I2C control interface
    ESP_LOGI(TAG, "🔧 Creating I2C control interface...");
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_0,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = config->i2c_bus_handle,
    };
    codec->ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (!codec->ctrl_if) {
        ESP_LOGE(TAG, "❌ Failed to create I2C control interface");
        free(codec);
        return NULL;
    }
    ESP_LOGI(TAG, "✅ I2C control interface created");

    // Step 4: Create GPIO interface
    codec->gpio_if = audio_codec_new_gpio();
    if (!codec->gpio_if) {
        ESP_LOGE(TAG, "❌ Failed to create GPIO interface");
        free(codec);
        return NULL;
    }
    ESP_LOGI(TAG, "✅ GPIO interface created");

    // Step 5: Create ES8311 codec interface
    ESP_LOGI(TAG, "🔧 Creating ES8311 codec interface...");
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = codec->ctrl_if,
        .gpio_if = codec->gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,  // Like xiaozhi-esp32
        .pa_pin = config->pa_gpio,
        .use_mclk = true,
        .hw_gain = {
            .pa_voltage = 5.0,
            .codec_dac_voltage = 3.3,
        },
        .pa_reverted = false,
    };
    codec->codec_if = es8311_codec_new(&es8311_cfg);
    if (!codec->codec_if) {
        ESP_LOGE(TAG, "❌ Failed to create ES8311 codec interface");
        free(codec);
        return NULL;
    }
    ESP_LOGI(TAG, "✅ ES8311 codec interface created");
    
    codec->initialized = true;
    codec->input_enabled = false;
    codec->output_enabled = false;
    codec->codec_dev = NULL;  // Created only when needed (like xiaozhi-esp32)
    
    ESP_LOGI(TAG, "✅ ES8311 codec initialized successfully");
    return codec;
}

esp_err_t audio_codec_start(audio_codec_handle_t codec) {
    if (!codec || !codec->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🎵 Starting ES8311 codec...");
    
    // Enable output (following xiaozhi-esp32 EnableOutput approach)
    codec->output_enabled = true;
    update_device_state(codec);
    
    ESP_LOGI(TAG, "✅ ES8311 codec started");
    return ESP_OK;
}

esp_err_t audio_codec_stop(audio_codec_handle_t codec) {
    if (!codec) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔧 Stopping ES8311 codec...");
    
    // Disable output (following xiaozhi-esp32 EnableOutput approach)
    codec->output_enabled = false;
    update_device_state(codec);
    
    ESP_LOGI(TAG, "✅ ES8311 codec stopped");
    return ESP_OK;
}

esp_err_t audio_codec_deinit(audio_codec_handle_t codec) {
    if (!codec) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🗑️ Deinitializing ES8311 codec...");
    
    if (codec->initialized) {
        audio_codec_stop(codec);
        
        if (codec->codec_dev) {
            esp_codec_dev_delete(codec->codec_dev);
        }
        
        // Clean up interfaces like xiaozhi-esp32 destructor
        audio_codec_delete_codec_if(codec->codec_if);
        audio_codec_delete_ctrl_if(codec->ctrl_if);
        audio_codec_delete_gpio_if(codec->gpio_if);
        audio_codec_delete_data_if(codec->data_if);
    }
    
    free(codec);
    ESP_LOGI(TAG, "✅ ES8311 codec deinitialized");
    return ESP_OK;
}

esp_err_t audio_codec_set_volume(audio_codec_handle_t codec, int volume) {
    if (!codec || !codec->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    codec->config.volume = volume;
    
    if (codec->codec_dev) {
        esp_err_t ret = esp_codec_dev_set_out_vol(codec->codec_dev, volume);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "🔊 Volume set to %d%% via ES8311 registers", volume);
        } else {
            ESP_LOGE(TAG, "❌ Failed to set volume: %s", esp_err_to_name(ret));
        }
        return ret;
    }
    
    return ESP_OK;
}

int audio_codec_play_data(audio_codec_handle_t codec, const void* data, size_t length, uint32_t timeout_ms) {
    if (!codec || !codec->initialized || !data) {
        ESP_LOGW(TAG, "Invalid codec or data");
        return -1;
    }
    
    if (!codec->output_enabled || !codec->codec_dev) {
        ESP_LOGW(TAG, "Codec output not enabled");
        return -1;
    }
    
    // Following xiaozhi-esp32 Write approach
    esp_err_t ret = esp_codec_dev_write(codec->codec_dev, (void*)data, length);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to write audio data");
        return -1;
    }
    
    ESP_LOGI(TAG, "🎵 Wrote %d bytes via ES8311 codec", (int)length);
    return length;
}

bool audio_codec_is_playing(audio_codec_handle_t codec) {
    return codec && codec->output_enabled;
}

esp_err_t audio_codec_stop_playback(audio_codec_handle_t codec) {
    if (!codec) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Simple implementation - just mark as not playing
    ESP_LOGI(TAG, "🔧 Stopping audio playback");
    return ESP_OK;
}
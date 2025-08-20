#include "audio_codec.h"
#include <esp_log.h>
#include <esp_err.h>
#include <string.h>
#include <stdlib.h>
#include <driver/i2s_std.h>
#include <driver/i2c.h>

// ESP Codec Dev framework includes
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

static const char *TAG = "AUDIO_CODEC_ES8311";

// ES8311 codec structure using ESP Codec Dev framework
struct audio_codec_s {
    audio_codec_config_t config;
    esp_codec_dev_handle_t codec_dev;
    bool initialized;
    bool playing;
    bool output_enabled;
};

audio_codec_handle_t audio_codec_init(const audio_codec_config_t* config) {
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return NULL;
    }
    
    ESP_LOGI(TAG, "🎵 Initializing ES8311 codec with ESP Codec Dev framework...");
    
    audio_codec_handle_t codec = calloc(1, sizeof(struct audio_codec_s));
    if (!codec) {
        ESP_LOGE(TAG, "Failed to allocate memory for codec");
        return NULL;
    }
    
    memcpy(&codec->config, config, sizeof(audio_codec_config_t));
    
    // Step 1: Create I2C control interface for ES8311
    ESP_LOGI(TAG, "🔧 Creating I2C control interface...");
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_0,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = config->i2c_bus_handle,  // Use provided I2C bus handle
    };
    const audio_codec_ctrl_if_t* ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (!ctrl_if) {
        ESP_LOGE(TAG, "❌ Failed to create I2C control interface");
        free(codec);
        return NULL;
    }
    
    // Step 2: Create GPIO interface for power amplifier  
    ESP_LOGI(TAG, "🔧 Creating GPIO interface...");
    const audio_codec_gpio_if_t* gpio_if = audio_codec_new_gpio();
    if (!gpio_if) {
        ESP_LOGE(TAG, "❌ Failed to create GPIO interface");
        free(codec);
        return NULL;
    }
    
    // Step 3: Create I2S channels matching xiaozhi-esp32 approach
    ESP_LOGI(TAG, "🔧 Creating I2S duplex channels...");
    i2s_chan_handle_t tx_handle = NULL;
    i2s_chan_handle_t rx_handle = NULL;
    
    // Create both TX and RX channels (duplex mode like xiaozhi-esp32)
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear = false,
    };
    
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to create I2S channels: %s", esp_err_to_name(ret));
        free(codec);
        return NULL;
    }
    
    // Configure I2S TX channel (matching xiaozhi-esp32 std_cfg)
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = config->sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,    // Use mono to match OGG decoder
            .slot_mask = I2S_STD_SLOT_LEFT,     // Left slot for mono
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
        },
        .gpio_cfg = {
            .mclk = config->mclk_gpio,
            .bclk = config->bclk_gpio,
            .ws = config->ws_gpio,
            .dout = config->dout_gpio,
            .din = config->din_gpio,
        },
    };
    
    ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize I2S TX channel: %s", esp_err_to_name(ret));
        if (tx_handle) i2s_del_channel(tx_handle);
        if (rx_handle) i2s_del_channel(rx_handle);
        free(codec);
        return NULL;
    }
    
    // Initialize RX channel with same config
    ret = i2s_channel_init_std_mode(rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize I2S RX channel: %s", esp_err_to_name(ret));
        if (tx_handle) i2s_del_channel(tx_handle);
        if (rx_handle) i2s_del_channel(rx_handle);
        free(codec);
        return NULL;
    }
    
    // Step 4: Create I2S data interface with pre-created duplex channels
    ESP_LOGI(TAG, "🔧 Creating I2S data interface with duplex channels...");
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle,  // Pass both RX handle for duplex mode
        .tx_handle = tx_handle,  // Pass both TX handle for duplex mode
    };
    const audio_codec_data_if_t* data_if = audio_codec_new_i2s_data(&i2s_cfg);
    if (!data_if) {
        ESP_LOGE(TAG, "❌ Failed to create I2S data interface");
        free(codec);
        return NULL;
    }
    
    // Step 5: Create ES8311 codec interface
    ESP_LOGI(TAG, "🔧 Creating ES8311 codec interface...");
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,  // Use BOTH mode like xiaozhi-esp32
        .pa_pin = config->pa_gpio,
        .use_mclk = true,
        .hw_gain = {
            .pa_voltage = 5.0,
            .codec_dac_voltage = 3.3,
        },
        .pa_reverted = false,
    };
    
    const audio_codec_if_t* codec_if = es8311_codec_new(&es8311_cfg);
    if (!codec_if) {
        ESP_LOGE(TAG, "❌ Failed to create ES8311 codec interface");
        free(codec);
        return NULL;
    }
    
    // Step 6: Create ESP codec device
    ESP_LOGI(TAG, "🔧 Creating ESP codec device...");
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,  // Use IN_OUT like xiaozhi-esp32
        .codec_if = codec_if,
        .data_if = data_if,
    };
    
    codec->codec_dev = esp_codec_dev_new(&dev_cfg);
    if (!codec->codec_dev) {
        ESP_LOGE(TAG, "❌ Failed to create ESP codec device");
        free(codec);
        return NULL;
    }
    
    codec->initialized = true;
    ESP_LOGI(TAG, "✅ ES8311 codec initialized successfully with ESP Codec Dev framework");
    ESP_LOGI(TAG, "🔧 Config: %dHz, %dbit, %dch, PA GPIO%d", 
             config->sample_rate, config->bits_per_sample, config->channels, config->pa_gpio);
    
    return codec;
}

esp_err_t audio_codec_start(audio_codec_handle_t codec) {
    if (!codec || !codec->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🎵 Starting ES8311 codec...");
    
    // Open codec device with audio format
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = codec->config.bits_per_sample,
        .channel = codec->config.channels,
        .channel_mask = 0,
        .sample_rate = codec->config.sample_rate,
        .mclk_multiple = 0,
    };
    
    esp_err_t ret = esp_codec_dev_open(codec->codec_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to open codec device: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set initial volume
    ret = esp_codec_dev_set_out_vol(codec->codec_dev, codec->config.volume);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to set output volume: %s", esp_err_to_name(ret));
        return ret;
    }
    
    codec->output_enabled = true;
    ESP_LOGI(TAG, "🎉 ES8311 codec started successfully with %d%% volume", codec->config.volume);
    return ESP_OK;
}

esp_err_t audio_codec_stop(audio_codec_handle_t codec) {
    if (!codec || !codec->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🛑 Stopping ES8311 codec...");
    
    esp_err_t ret = esp_codec_dev_close(codec->codec_dev);
    codec->playing = false;
    codec->output_enabled = false;
    
    ESP_LOGI(TAG, "✅ ES8311 codec stopped");
    return ret;
}

esp_err_t audio_codec_deinit(audio_codec_handle_t codec) {
    if (!codec) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🗑️ Deinitializing ES8311 codec...");
    
    if (codec->initialized) {
        audio_codec_stop(codec);
        esp_codec_dev_delete(codec->codec_dev);
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
    
    esp_err_t ret = esp_codec_dev_set_out_vol(codec->codec_dev, volume);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "🔊 Volume set to %d%% via ES8311 registers", volume);
    } else {
        ESP_LOGE(TAG, "❌ Failed to set volume: %s", esp_err_to_name(ret));
    }
    
    return ret;
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
    
    // ESP Codec Dev write API only takes 3 parameters: dev, data, length
    int bytes_written = esp_codec_dev_write(codec->codec_dev, (void*)data, length);
    
    if (bytes_written < 0) {
        ESP_LOGE(TAG, "Failed to write audio data");
        return -1;
    }
    
    ESP_LOGI(TAG, "🎵 Wrote %d bytes via ES8311 codec", bytes_written);
    return bytes_written;
}

bool audio_codec_is_playing(audio_codec_handle_t codec) {
    return codec && codec->playing;
}

int audio_codec_write_raw(audio_codec_handle_t codec, const void* data, size_t length) {
    if (!codec || !codec->initialized || !data) {
        ESP_LOGW(TAG, "Invalid codec or data");
        return -1;
    }
    
    if (!codec->output_enabled) {
        ESP_LOGW(TAG, "Codec output not enabled");
        return -1;
    }
    
    codec->playing = true;
    
    // Use ESP Codec Dev write API directly
    int bytes_written = esp_codec_dev_write(codec->codec_dev, (void*)data, length);
    
    if (bytes_written < 0) {
        ESP_LOGE(TAG, "Failed to write raw audio data");
        return -1;
    }
    
    ESP_LOGI(TAG, "🎵 Raw write: %d bytes via ES8311 codec", bytes_written);
    return bytes_written;
}
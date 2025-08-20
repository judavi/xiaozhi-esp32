#include "sound_manager.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <driver/gpio.h>
#include <driver/i2c_master.h>

// Include embedded audio samples and language config
#include "assets/lang_config.h"
#include "ogg_decoder.h"
#include "esp_codec_dev.h"

static const char *TAG = "SOUND_MANAGER";

// Audio codec configuration based on SP-ESP32-S3-1.28-BOX board
// GPIO pins from the reference project config.h
#define MCLK_GPIO          16
#define BCLK_GPIO          9
#define WS_GPIO            45
#define DOUT_GPIO          8   // DIN in xiaozhi-esp32, swapped
#define DIN_GPIO           10  // DOUT in xiaozhi-esp32, swapped
#define I2C_SDA_GPIO       15
#define I2C_SCL_GPIO       14
#define PA_GPIO            46

static audio_codec_handle_t codec_handle = NULL;
static bool initialized = false;
static i2c_master_bus_handle_t i2c_bus_handle = NULL;

void sound_manager_init(void) {
    ESP_LOGI(TAG, "🔊 Initializing Sound Manager with ES8311 Audio Codec...");
    ESP_LOGI(TAG, "🔧 GPIO Config: MCLK=%d, BCLK=%d, WS=%d, DOUT=%d, PA=%d", 
             MCLK_GPIO, BCLK_GPIO, WS_GPIO, DOUT_GPIO, PA_GPIO);
    ESP_LOGI(TAG, "🔧 I2C Config: SDA=%d, SCL=%d", I2C_SDA_GPIO, I2C_SCL_GPIO);
    
    // Step 1: Initialize I2C master bus for ES8311 communication
    ESP_LOGI(TAG, "🔧 Initializing I2C master bus...");
    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_SCL_GPIO,
        .sda_io_num = I2C_SDA_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_conf, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to create I2C master bus: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "✅ I2C master bus created successfully");
    
    // Step 2: Configure audio codec
    audio_codec_config_t codec_config = {
        .mclk_gpio = MCLK_GPIO,
        .bclk_gpio = BCLK_GPIO,
        .ws_gpio = WS_GPIO,
        .dout_gpio = DOUT_GPIO,
        .din_gpio = DIN_GPIO,
        .sda_gpio = I2C_SDA_GPIO,
        .scl_gpio = I2C_SCL_GPIO,
        .pa_gpio = PA_GPIO,
        .sample_rate = 24000,   // 24kHz to match board configuration
        .bits_per_sample = 16,
        .channels = 2,          // Stereo to match I2S slot configuration
        .volume = 80,           // 80% volume
        .i2c_bus_handle = i2c_bus_handle  // Pass I2C bus handle to codec
    };
    
    ESP_LOGI(TAG, "🔧 About to initialize audio codec...");
    
    // Try to initialize codec with error handling
    codec_handle = audio_codec_init(&codec_config);
    if (codec_handle) {
        ESP_LOGI(TAG, "✅ Audio codec init successful, starting codec...");
        esp_err_t ret = audio_codec_start(codec_handle);
        if (ret == ESP_OK) {
            initialized = true;
            ESP_LOGI(TAG, "🎉 Sound Manager initialized with ES8311 codec successfully!");
            ESP_LOGI(TAG, "🔧 Ready for audio playback - ES8311 codec configured, PA on GPIO%d", PA_GPIO);
        } else {
            ESP_LOGE(TAG, "❌ Failed to start audio codec: %s", esp_err_to_name(ret));
            audio_codec_deinit(codec_handle);
            codec_handle = NULL;
            initialized = false;
        }
    } else {
        ESP_LOGE(TAG, "❌ Failed to initialize audio codec - check I2C connection and ES8311 chip");
        initialized = false;
    }
    
    // If codec initialization failed, clean up I2C bus but don't crash
    if (!initialized) {
        ESP_LOGW(TAG, "⚠️ Audio codec initialization failed, continuing without audio support");
        if (i2c_bus_handle) {
            // Don't delete the I2C bus as other components might use it
            ESP_LOGI(TAG, "🔧 I2C bus left available for other components");
        }
    }
}

static void play_test_pcm_data(const char* name) {
    if (!initialized || !codec_handle) {
        ESP_LOGW(TAG, "⚠️ Audio codec not ready for playback");
        return;
    }
    
    ESP_LOGI(TAG, "🎵 Playing test PCM data: %s", name);
    
    // Create simple test PCM data - a 1kHz sine wave for 1 second at 24kHz
    const size_t sample_count = 24000;  // 1 second at 24kHz
    int16_t* test_data = malloc(sample_count * sizeof(int16_t));
    if (!test_data) {
        ESP_LOGE(TAG, "❌ Failed to allocate test data");
        return;
    }
    
    // Generate 1kHz sine wave
    for (size_t i = 0; i < sample_count; i++) {
        double t = (double)i / 24000.0;  // Time in seconds
        double sample = sin(2.0 * M_PI * 1000.0 * t) * 16000.0;  // 1kHz sine wave, scaled to 16-bit
        test_data[i] = (int16_t)sample;
    }
    
    // Try to play the test data
    size_t bytes_to_play = sample_count * sizeof(int16_t);
    int written = audio_codec_play_data(codec_handle, test_data, bytes_to_play, 5000);
    
    if (written < 0) {
        ESP_LOGE(TAG, "❌ Failed to play test PCM data");
    } else {
        ESP_LOGI(TAG, "✅ Test PCM data: wrote %d bytes", written);
        // Wait for playback to complete
        vTaskDelay(pdMS_TO_TICKS(1100));
    }
    
    free(test_data);
}

static void play_ogg_audio(const uint8_t* ogg_data, size_t ogg_size, const char* name) {
    if (!initialized || !codec_handle) {
        ESP_LOGW(TAG, "⚠️ Audio codec not ready for playback");
        return;
    }
    
    ESP_LOGI(TAG, "🎵 Playing OGG audio: %s (%u bytes)", name, ogg_size);
    
    // Initialize OGG decoder
    ogg_decoder_handle_t decoder = ogg_decoder_init(ogg_data, ogg_size);
    if (!decoder) {
        ESP_LOGE(TAG, "❌ Failed to initialize OGG decoder for %s", name);
        return;
    }
    
    // Get audio format info
    uint32_t sample_rate;
    uint8_t channels;
    ogg_decoder_get_info(decoder, &sample_rate, &channels);
    ESP_LOGI(TAG, "🔍 OGG format: %dHz, %dch", sample_rate, channels);
    
    // Decode and play in chunks
    const size_t chunk_samples = 1024;  // Process 1024 samples at a time
    int16_t* pcm_buffer = malloc(chunk_samples * sizeof(int16_t));
    if (!pcm_buffer) {
        ESP_LOGE(TAG, "❌ Failed to allocate PCM buffer");
        ogg_decoder_deinit(decoder);
        return;
    }
    
    size_t total_samples_played = 0;
    while (!ogg_decoder_is_finished(decoder)) {
        size_t samples_decoded;
        esp_err_t ret = ogg_decoder_decode(decoder, pcm_buffer, 
                                         chunk_samples * sizeof(int16_t), &samples_decoded);
        
        if (ret != ESP_OK || samples_decoded == 0) {
            ESP_LOGW(TAG, "⚠️ OGG decoding finished or error occurred");
            break;
        }
        
        // Convert mono to stereo by duplicating samples
        int16_t* stereo_buffer = malloc(samples_decoded * 2 * sizeof(int16_t));
        if (!stereo_buffer) {
            ESP_LOGE(TAG, "❌ Failed to allocate stereo buffer");
            break;
        }
        
        for (size_t i = 0; i < samples_decoded; i++) {
            stereo_buffer[i * 2] = pcm_buffer[i];     // Left channel
            stereo_buffer[i * 2 + 1] = pcm_buffer[i]; // Right channel (duplicate)
        }
        
        // Play the converted stereo PCM data
        size_t bytes_to_play = samples_decoded * 2 * sizeof(int16_t);
        int written = audio_codec_play_data(codec_handle, stereo_buffer, bytes_to_play, 5000);
        
        free(stereo_buffer);
        
        if (written < 0) {
            ESP_LOGE(TAG, "❌ Failed to play decoded audio chunk");
            break;
        }
        
        total_samples_played += samples_decoded;
        ESP_LOGD(TAG, "🎵 Played %d samples (total: %d)", samples_decoded, total_samples_played);
        
        // Small delay to allow audio processing
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "✅ Finished playing OGG audio: %s (%d total samples)", name, total_samples_played);
    
    // Cleanup
    free(pcm_buffer);
    ogg_decoder_deinit(decoder);
}

static void play_welcome_beeps(void) {
    if (!initialized || !codec_handle) {
        ESP_LOGW(TAG, "⚠️ Audio codec not ready for playback");
        return;
    }
    
    ESP_LOGI(TAG, "🎵 Playing welcome beep sequence");
    
    // Create a welcome beep pattern: 3 ascending tones + silence
    const int frequencies[] = {800, 1000, 1200}; // Hz
    const double duration = 0.5; // seconds per beep
    const double silence_duration = 0.2; // seconds of silence at end
    const size_t sample_rate = 24000;
    const size_t samples_per_beep = (size_t)(sample_rate * duration);
    const size_t silence_samples = (size_t)(sample_rate * silence_duration);
    const size_t total_samples = samples_per_beep * 3 + silence_samples; // 3 beeps + silence
    
    int16_t* beep_data = malloc(total_samples * 2 * sizeof(int16_t));  // Stereo
    if (!beep_data) {
        ESP_LOGE(TAG, "❌ Failed to allocate beep data");
        return;
    }
    
    // Generate 3 beeps
    for (int beep = 0; beep < 3; beep++) {
        size_t start_sample = beep * samples_per_beep;
        int freq = frequencies[beep];
        
        for (size_t i = 0; i < samples_per_beep; i++) {
            double t = (double)i / sample_rate;
            // Apply envelope to avoid clicks (fade in/out)
            double envelope = 1.0;
            if (i < 1000) envelope = (double)i / 1000.0; // Fade in
            if (i > samples_per_beep - 1000) envelope = (double)(samples_per_beep - i) / 1000.0; // Fade out
            
            double sample = sin(2.0 * M_PI * freq * t) * envelope * 6000.0;
            int16_t sample_16 = (int16_t)sample;
            
            size_t stereo_index = (start_sample + i) * 2;
            beep_data[stereo_index] = sample_16;      // Left channel
            beep_data[stereo_index + 1] = sample_16;  // Right channel
        }
    }
    
    // Add silence at the end to prevent noise
    for (size_t i = samples_per_beep * 3; i < total_samples; i++) {
        size_t stereo_index = i * 2;
        beep_data[stereo_index] = 0;      // Left channel silence
        beep_data[stereo_index + 1] = 0;  // Right channel silence
    }
    
    // Play the beeps
    size_t bytes_to_play = total_samples * 2 * sizeof(int16_t);
    int written = audio_codec_play_data(codec_handle, beep_data, bytes_to_play, 5000);
    
    if (written > 0) {
        ESP_LOGI(TAG, "✅ Welcome beeps played successfully: wrote %d bytes", written);
    } else {
        ESP_LOGE(TAG, "❌ Failed to play welcome beeps");
    }
    
    free(beep_data);
}

bool sound_manager_play_sound(sound_type_t sound_type) {
    if (!initialized || !codec_handle) {
        ESP_LOGW(TAG, "⚠️ Sound manager not initialized");
        return false;
    }
    
    ESP_LOGI(TAG, "🔊 Playing sound type: %d", sound_type);
    
    switch (sound_type) {
        case SOUND_TYPE_WELCOME:
            play_welcome_beeps();
            return true;
        case SOUND_TYPE_WELCOME_OGG:
            play_ogg_audio(welcome_ogg_start, WELCOME_OGG_SIZE(), "welcome.ogg");
            return true;
        default:
            ESP_LOGE(TAG, "❌ Unknown sound type: %d", sound_type);
            return false;
    }
}

static void play_test_tone(void) {
    if (!initialized || !codec_handle) {
        ESP_LOGW(TAG, "⚠️ Audio codec not ready for playback");
        return;
    }
    
    ESP_LOGI(TAG, "🎵 Playing test tone (1kHz sine wave for 2 seconds)");
    
    // Generate a 1kHz sine wave for 2 seconds at 24kHz sample rate + silence
    const size_t audio_samples = 24000 * 2;  // 2 seconds of audio
    const size_t silence_samples = 24000 * 0.2; // 0.2 seconds of silence
    const size_t sample_count = audio_samples + silence_samples;
    int16_t* tone_data = malloc(sample_count * 2 * sizeof(int16_t));  // Stereo
    if (!tone_data) {
        ESP_LOGE(TAG, "❌ Failed to allocate tone data");
        return;
    }
    
    // Generate stereo sine wave for audio portion
    for (size_t i = 0; i < audio_samples; i++) {
        double t = (double)i / 24000.0;  // Time in seconds
        double sample = sin(2.0 * M_PI * 1000.0 * t) * 8000.0;  // 1kHz sine wave, scaled to 16-bit
        int16_t sample_16 = (int16_t)sample;
        
        tone_data[i * 2] = sample_16;      // Left channel
        tone_data[i * 2 + 1] = sample_16;  // Right channel
    }
    
    // Add silence at the end
    for (size_t i = audio_samples; i < sample_count; i++) {
        tone_data[i * 2] = 0;      // Left channel silence
        tone_data[i * 2 + 1] = 0;  // Right channel silence
    }
    
    // Play the tone
    size_t bytes_to_play = sample_count * 2 * sizeof(int16_t);
    int written = audio_codec_play_data(codec_handle, tone_data, bytes_to_play, 10000);
    
    if (written > 0) {
        ESP_LOGI(TAG, "✅ Test tone played successfully: wrote %d bytes", written);
    } else {
        ESP_LOGE(TAG, "❌ Failed to play test tone");
    }
    
    free(tone_data);
}

bool sound_manager_test_sound(const char* sound_name) {
    if (!sound_name) return false;
    
    if (strcmp(sound_name, "tone") == 0) {
        play_test_tone();
        return true;
    } else if (strcmp(sound_name, "welcome") == 0) {
        return sound_manager_play_sound(SOUND_TYPE_WELCOME);
    } else if (strcmp(sound_name, "welcome_ogg") == 0) {
        return sound_manager_play_sound(SOUND_TYPE_WELCOME_OGG);
    } else {
        ESP_LOGW(TAG, "⚠️ Unknown sound name: %s (supports 'tone', 'welcome', 'welcome_ogg')", sound_name);
        return false;
    }
}

char* sound_manager_get_test_result_json(bool success, const char* sound_name) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", success);
    cJSON_AddStringToObject(root, "sound_type", sound_name ? sound_name : "unknown");
    cJSON_AddStringToObject(root, "message", success ? "Sound played successfully" : "Failed to play sound");
    
    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);
    
    return json_string;
}

audio_codec_handle_t sound_manager_get_codec_handle(void) {
    return codec_handle;
}
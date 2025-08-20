#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio codec configuration
typedef struct {
    // I2S GPIO pins
    int mclk_gpio;      // Master clock
    int bclk_gpio;      // Bit clock
    int ws_gpio;        // Word select (LRCK)
    int dout_gpio;      // Data out
    int din_gpio;       // Data in (not used for output only)
    
    // I2C GPIO pins for codec control
    int sda_gpio;       // I2C data
    int scl_gpio;       // I2C clock
    
    // Power amplifier control
    int pa_gpio;        // Power amplifier enable pin
    
    // Audio parameters
    uint32_t sample_rate;   // Sample rate in Hz (e.g., 16000, 44100)
    uint8_t bits_per_sample; // 16 or 24 bits
    uint8_t channels;        // 1 (mono) or 2 (stereo)
    
    // Volume control
    int volume;         // 0-100
    
    // I2C bus handle for ES8311 communication
    void* i2c_bus_handle;   // i2c_master_bus_handle_t cast to void*
} audio_codec_config_t;

// Audio codec handle
typedef struct audio_codec_s* audio_codec_handle_t;

/**
 * @brief Initialize the audio codec
 * @param config Audio codec configuration
 * @return Handle to the audio codec or NULL on failure
 */
audio_codec_handle_t audio_codec_init(const audio_codec_config_t* config);

/**
 * @brief Start the audio codec
 * @param codec Audio codec handle
 * @return ESP_OK on success
 */
esp_err_t audio_codec_start(audio_codec_handle_t codec);

/**
 * @brief Stop the audio codec
 * @param codec Audio codec handle
 * @return ESP_OK on success
 */
esp_err_t audio_codec_stop(audio_codec_handle_t codec);

/**
 * @brief Deinitialize the audio codec
 * @param codec Audio codec handle
 * @return ESP_OK on success
 */
esp_err_t audio_codec_deinit(audio_codec_handle_t codec);

/**
 * @brief Set output volume
 * @param codec Audio codec handle
 * @param volume Volume level (0-100)
 * @return ESP_OK on success
 */
esp_err_t audio_codec_set_volume(audio_codec_handle_t codec, int volume);

/**
 * @brief Play audio data
 * @param codec Audio codec handle
 * @param data PCM audio data (16-bit samples)
 * @param length Number of bytes to play
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes written or -1 on error
 */
int audio_codec_play_data(audio_codec_handle_t codec, const void* data, size_t length, uint32_t timeout_ms);

/**
 * @brief Write raw audio data directly (for testing)
 * @param codec Audio codec handle
 * @param data PCM audio data
 * @param length Number of bytes to write
 * @return Number of bytes written or -1 on error
 */
int audio_codec_write_raw(audio_codec_handle_t codec, const void* data, size_t length);

/**
 * @brief Check if codec is playing
 * @param codec Audio codec handle
 * @return true if playing, false otherwise
 */
bool audio_codec_is_playing(audio_codec_handle_t codec);

/**
 * @brief Stop audio playback and disable I2S transmission
 * @param codec Audio codec handle
 * @return ESP_OK on success
 */
esp_err_t audio_codec_stop_playback(audio_codec_handle_t codec);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_CODEC_H
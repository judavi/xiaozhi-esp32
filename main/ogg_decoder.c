#include "ogg_decoder.h"
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "OGG_DECODER";

// Simple OGG decoder structure for now - will implement basic decoding
struct ogg_decoder_s {
    const uint8_t* ogg_data;
    size_t ogg_size;
    size_t current_pos;
    uint32_t sample_rate;
    uint8_t channels;
    bool finished;
    bool initialized;
};

ogg_decoder_handle_t ogg_decoder_init(const uint8_t* ogg_data, size_t ogg_size) {
    if (!ogg_data || ogg_size == 0) {
        ESP_LOGE(TAG, "Invalid OGG data");
        return NULL;
    }
    
    ESP_LOGI(TAG, "🎵 Initializing OGG decoder for %d bytes", ogg_size);
    
    ogg_decoder_handle_t decoder = calloc(1, sizeof(struct ogg_decoder_s));
    if (!decoder) {
        ESP_LOGE(TAG, "Failed to allocate decoder");
        return NULL;
    }
    
    decoder->ogg_data = ogg_data;
    decoder->ogg_size = ogg_size;
    decoder->current_pos = 0;
    decoder->finished = false;
    
    // For now, assume standard audio format matching our codec
    decoder->sample_rate = 24000;  // Match our I2S configuration
    decoder->channels = 1;         // Mono
    decoder->initialized = true;
    
    ESP_LOGI(TAG, "✅ OGG decoder initialized: %dHz, %dch", 
             decoder->sample_rate, decoder->channels);
    
    return decoder;
}

esp_err_t ogg_decoder_decode(ogg_decoder_handle_t decoder, int16_t* pcm_buffer, 
                           size_t buffer_size, size_t* samples_decoded) {
    if (!decoder || !decoder->initialized || !pcm_buffer || !samples_decoded) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (decoder->finished) {
        *samples_decoded = 0;
        return ESP_OK;
    }
    
    // Simple temporary implementation: 
    // Skip OGG header and convert remaining data to PCM-like format
    // This is NOT proper OGG decoding but will produce some audio output for testing
    
    size_t bytes_per_sample = sizeof(int16_t) * decoder->channels;
    size_t remaining_bytes = decoder->ogg_size - decoder->current_pos;
    
    if (remaining_bytes == 0) {
        decoder->finished = true;
        *samples_decoded = 0;
        return ESP_OK;
    }
    
    // Skip OGG header on first decode (simple heuristic)
    if (decoder->current_pos == 0) {
        // Look for the first occurrence of audio-like data (skip ~1KB of headers)
        size_t header_skip = (decoder->ogg_size > 1024) ? 1024 : decoder->ogg_size / 4;
        decoder->current_pos = header_skip;
        remaining_bytes = decoder->ogg_size - decoder->current_pos;
        ESP_LOGI(TAG, "🔍 Skipping %d bytes of OGG headers", header_skip);
    }
    
    size_t bytes_to_copy = (remaining_bytes < buffer_size) ? remaining_bytes : buffer_size;
    size_t samples_to_generate = bytes_to_copy / bytes_per_sample;
    
    // Simple conversion: use OGG data as seed for PCM-like audio
    // This will sound distorted but should produce audible output
    for (size_t i = 0; i < samples_to_generate && decoder->current_pos < decoder->ogg_size; i++) {
        uint8_t ogg_byte = decoder->ogg_data[decoder->current_pos++];
        
        // Convert byte to 16-bit PCM sample with some audio characteristics
        // Apply simple filtering to make it more audio-like
        int16_t sample = (int16_t)((ogg_byte - 128) * 256);
        
        // Apply simple low-pass filtering to reduce noise
        if (i > 0) {
            sample = (sample + pcm_buffer[i-1]) / 2;
        }
        
        pcm_buffer[i] = sample;
    }
    
    *samples_decoded = samples_to_generate;
    
    if (decoder->current_pos >= decoder->ogg_size) {
        decoder->finished = true;
        ESP_LOGI(TAG, "🏁 OGG decoding finished");
    }
    
    ESP_LOGD(TAG, "🎵 Decoded %d samples from OGG data", samples_to_generate);
    return ESP_OK;
}

esp_err_t ogg_decoder_get_info(ogg_decoder_handle_t decoder, uint32_t* sample_rate, uint8_t* channels) {
    if (!decoder || !decoder->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (sample_rate) *sample_rate = decoder->sample_rate;
    if (channels) *channels = decoder->channels;
    
    return ESP_OK;
}

bool ogg_decoder_is_finished(ogg_decoder_handle_t decoder) {
    return decoder ? decoder->finished : true;
}

void ogg_decoder_deinit(ogg_decoder_handle_t decoder) {
    if (decoder) {
        ESP_LOGI(TAG, "🗑️ Cleaning up OGG decoder");
        free(decoder);
    }
}
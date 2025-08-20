#ifndef OGG_DECODER_H
#define OGG_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ogg_decoder_s* ogg_decoder_handle_t;

/**
 * @brief Initialize OGG Vorbis decoder
 * @param ogg_data Pointer to OGG file data
 * @param ogg_size Size of OGG file data
 * @return Handle to decoder or NULL on failure
 */
ogg_decoder_handle_t ogg_decoder_init(const uint8_t* ogg_data, size_t ogg_size);

/**
 * @brief Decode OGG data to PCM samples
 * @param decoder Decoder handle
 * @param pcm_buffer Output buffer for PCM samples (16-bit)
 * @param buffer_size Size of output buffer in bytes
 * @param samples_decoded Number of samples decoded (output)
 * @return ESP_OK on success, ESP_ERR_* on error
 */
esp_err_t ogg_decoder_decode(ogg_decoder_handle_t decoder, int16_t* pcm_buffer, 
                           size_t buffer_size, size_t* samples_decoded);

/**
 * @brief Get audio format information
 * @param decoder Decoder handle
 * @param sample_rate Sample rate (output)
 * @param channels Number of channels (output)
 * @return ESP_OK on success
 */
esp_err_t ogg_decoder_get_info(ogg_decoder_handle_t decoder, uint32_t* sample_rate, uint8_t* channels);

/**
 * @brief Check if decoding is finished
 * @param decoder Decoder handle
 * @return true if finished, false otherwise
 */
bool ogg_decoder_is_finished(ogg_decoder_handle_t decoder);

/**
 * @brief Clean up decoder
 * @param decoder Decoder handle
 */
void ogg_decoder_deinit(ogg_decoder_handle_t decoder);

#ifdef __cplusplus
}
#endif

#endif // OGG_DECODER_H
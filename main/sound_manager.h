#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include <stdbool.h>
#include "audio_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOUND_TYPE_BEEP,
    SOUND_TYPE_BUZZER,
    SOUND_TYPE_CHIME,
    SOUND_TYPE_ALERT,
    SOUND_TYPE_MELODY,
    SOUND_TYPE_WELCOME,
    SOUND_TYPE_WELCOME_OGG
} sound_type_t;

// Initialize sound manager
void sound_manager_init(void);

// Play a specific sound type
bool sound_manager_play_sound(sound_type_t sound_type);

// Test sound by name
bool sound_manager_test_sound(const char* sound_name);

// Get JSON representation of sound test result
char* sound_manager_get_test_result_json(bool success, const char* sound_name);

// Get codec handle for volume control
audio_codec_handle_t sound_manager_get_codec_handle(void);

#ifdef __cplusplus
}
#endif

#endif // SOUND_MANAGER_H
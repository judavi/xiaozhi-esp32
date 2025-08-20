# Language selection - default to English
if(NOT DEFINED CONFIG_LANGUAGE)
    set(CONFIG_LANGUAGE "en-US")
endif()

# Define language directory based on configuration
if(CONFIG_LANGUAGE STREQUAL "es-ES")
    set(LANG_DIR "es-ES")
else()
    set(LANG_DIR "en-US")
endif()

# Collect OGG sound files based on language selection
file(GLOB OGG_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/${LANG_DIR}/*.ogg)

# Optional: Show which files will be embedded (uncomment for debug)
# message(STATUS "Language directory: ${LANG_DIR}")
# message(STATUS "OGG sounds found: ${OGG_SOUNDS}")

idf_component_register(
    SRCS 
        "main.c"
        "simple_wifi.cpp"
        "time_sync.cpp"
        "http_server.cpp"
        "alarm_manager.cpp"
        "battery_monitor.c"
        "sound_manager.c"
        "audio_codec_es8311.c"
        "ogg_decoder.c"
    INCLUDE_DIRS 
        "."
    EMBED_FILES
        "../www/index.html"
        "../www/style.css"
        "../www/script.js"
        ${OGG_SOUNDS}
    REQUIRES 
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        esp_http_server
        lwip
        json
        esp_adc
        esp_driver_gpio
        esp_driver_i2s
        esp_driver_i2c
        esp_codec_dev
) 
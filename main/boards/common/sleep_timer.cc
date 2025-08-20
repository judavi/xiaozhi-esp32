#include "sleep_timer.h"
#include "application.h"
#include "board.h"
#include "display.h"

#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_lvgl_port.h>

#define TAG "SleepTimer"


SleepTimer::SleepTimer(int seconds_to_light_sleep, int seconds_to_deep_sleep)
    : seconds_to_light_sleep_(seconds_to_light_sleep), seconds_to_deep_sleep_(seconds_to_deep_sleep) {
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            auto self = static_cast<SleepTimer*>(arg);
            self->CheckTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sleep_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &sleep_timer_));
}

SleepTimer::~SleepTimer() {
    esp_timer_stop(sleep_timer_);
    esp_timer_delete(sleep_timer_);
}

void SleepTimer::SetEnabled(bool enabled) {
    if (enabled && !enabled_) {
        ticks_ = 0;
        enabled_ = enabled;
        ESP_ERROR_CHECK(esp_timer_start_periodic(sleep_timer_, 1000000));
        ESP_LOGI(TAG, "Sleep timer enabled");
    } else if (!enabled && enabled_) {
        ESP_ERROR_CHECK(esp_timer_stop(sleep_timer_));
        enabled_ = enabled;
        WakeUp();
        ESP_LOGI(TAG, "Sleep timer disabled");
    }
}

void SleepTimer::OnEnterLightSleepMode(std::function<void()> callback) {
    on_enter_light_sleep_mode_ = callback;
}

void SleepTimer::OnExitLightSleepMode(std::function<void()> callback) {
    on_exit_light_sleep_mode_ = callback;
}

void SleepTimer::OnEnterDeepSleepMode(std::function<void()> callback) {
    on_enter_deep_sleep_mode_ = callback;
}

void SleepTimer::CheckTimer() {
    auto& app = Application::GetInstance();
    
    // Simplified sleep check for alarm clock - just check if device is idle
    auto device_state = app.GetDeviceState();
    if (device_state != kDeviceStateIdle) {
        ResetTimer();
        return;
    }

    // Check if sleep timer has expired
    auto now = esp_timer_get_time() / 1000; // Convert to milliseconds
    if (now - last_activity_time_ >= sleep_timeout_ms_) {
        ESP_LOGI(TAG, "Sleep timer expired, entering deep sleep");
        // Enter deep sleep directly for alarm clock
        EnableDeepSleep();
    }
}

void SleepTimer::WakeUp() {
    ticks_ = 0;
    if (in_light_sleep_mode_) {
        in_light_sleep_mode_ = false;
        if (on_exit_light_sleep_mode_) {
            on_exit_light_sleep_mode_();
        }
    }
}

void SleepTimer::ResetTimer() {
    last_activity_time_ = esp_timer_get_time() / 1000; // Convert to milliseconds
}

void SleepTimer::EnableDeepSleep() {
    if (on_enter_deep_sleep_mode_) {
        on_enter_deep_sleep_mode_();
    }
    esp_deep_sleep_start();
}

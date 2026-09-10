// sim/stubs/esp_sleep.h —— host stub mirroring ESP-IDF API subset.
#pragma once
#include <stdint.h>

typedef enum {
    ESP_SLEEP_WAKEUP_UNDEFINED = 0,
    ESP_SLEEP_WAKEUP_EXT0,
    ESP_SLEEP_WAKEUP_EXT1,
    ESP_SLEEP_WAKEUP_TIMER,
    ESP_SLEEP_WAKEUP_TOUCHPAD,
    ESP_SLEEP_WAKEUP_ULP,
    ESP_SLEEP_WAKEUP_GPIO,
    ESP_SLEEP_WAKEUP_UART,
} esp_sleep_source_t;

static inline esp_sleep_source_t esp_sleep_get_wakeup_cause(void)
{
    return ESP_SLEEP_WAKEUP_UNDEFINED;
}

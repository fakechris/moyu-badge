// sim/stubs/esp_system.h —— host stub mirroring ESP-IDF API subset.
#pragma once
#include "esp_err.h"

typedef enum {
    ESP_RESET_UNKNOWN = 0,
    ESP_RESET_POWERON,
    ESP_RESET_SW,
    ESP_RESET_PANIC,
    ESP_RESET_INT_WDT,
    ESP_RESET_TASK_WDT,
    ESP_RESET_WDT,
    ESP_RESET_DEEPSLEEP,
    ESP_RESET_BROWNOUT,
    ESP_RESET_SDIO,
} esp_reset_reason_t;

static inline esp_reset_reason_t esp_reset_reason(void)
{
    return ESP_RESET_UNKNOWN;
}

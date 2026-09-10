// sim/stubs/esp_err.h —— host stub mirroring ESP-IDF API subset.
#pragma once
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL 1
#define ESP_ERR_NO_MEM 2
#define ESP_ERR_INVALID_STATE 3
#define ESP_ERR_NOT_FOUND 4
#define ESP_ERR_NVS_NO_FREE_PAGES 5
#define ESP_ERR_NVS_NEW_VERSION_FOUND 6

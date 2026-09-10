// sim/stubs/bsp_display.h —— same LVGL-facing API as upstream (SDL backend).
#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
struct _lv_display_t;
esp_err_t bsp_display_init(void);
void bsp_display_backlight(uint8_t percent);
struct _lv_display_t *bsp_lvgl_init(void);
bool bsp_lvgl_lock(int timeout_ms);
void bsp_lvgl_unlock(void);

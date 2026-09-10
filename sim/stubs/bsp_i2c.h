// sim/stubs/bsp_i2c.h — host stub (API subset used by app_main).
#pragma once
#include "esp_err.h"
esp_err_t bsp_i2c_init(void);
void bsp_i2c_scan(void);

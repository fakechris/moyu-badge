// sim/stubs/bsp_battery.h — host stub (API subset used by app_main).
#pragma once
#include "esp_err.h"
esp_err_t bsp_battery_init(void);
int bsp_battery_soc(void);

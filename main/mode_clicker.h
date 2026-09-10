// main/mode_clicker.h —— BLE PPT clicker mode.
#pragma once

#include "bsp_button.h"

void mode_clicker_enter(void);
void mode_clicker_exit(void);
void mode_clicker_key(bsp_btn_t btn, bsp_btn_ev_t ev);

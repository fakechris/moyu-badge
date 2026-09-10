// main/mode_pomo.h —— pomodoro mode (pomo_lite + LVGL face).
#pragma once

#include "bsp_button.h"

void mode_pomo_enter(void);
void mode_pomo_exit(void);
void mode_pomo_key(bsp_btn_t btn, bsp_btn_ev_t ev);

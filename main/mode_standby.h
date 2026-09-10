// main/mode_standby.h —— standby: dim screen, Beijing wall clock, sleep face;
// 30s idle without a USB host -> deep sleep (any button wakes to s_prev).
#pragma once

#include "bsp_button.h"

#include <stdbool.h>

void mode_standby_enter(void);
void mode_standby_exit(void);
void mode_standby_key(bsp_btn_t btn, bsp_btn_ev_t ev);
// Feed user input so the 30s deep-sleep timer restarts.
void mode_standby_activity(void);
// Returns true when the key was consumed for slot browsing (shell must not wake).
bool mode_standby_browse(bsp_btn_t btn, bsp_btn_ev_t ev);

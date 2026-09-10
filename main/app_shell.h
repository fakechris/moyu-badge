// main/app_shell.h —— top-level 5-mode framework (DESIGN: one OK-long rule).
// Mode 0 game (OC dungeon) / 1 pomodoro / 2 BLE clicker / 3 vocab / 4 standby.
// OK-long anywhere -> switcher overlay. All other keys -> active mode.
#pragma once

#include "bsp_button.h"

typedef enum {
    APP_MODE_GAME = 0,
    APP_MODE_POMO,
    APP_MODE_CLICKER,
    APP_MODE_VOCAB,
    APP_MODE_STANDBY,
    APP_MODE_SETTINGS,          // mute / language; never persisted as boot mode
    APP_MODE_COUNT,
    APP_MODE_SWITCHER = 0x80,  // overlay, not persisted
} app_mode_t;

void app_shell_boot(void);  // replaces direct demo enter in app_main
void app_shell_key(bsp_btn_t btn, bsp_btn_ev_t ev);
app_mode_t app_shell_mode(void);
void app_shell_enter(app_mode_t mode);  // persists (except switcher/standby return)
// DESIGN RULE: modes never auto-switch each other. The only state changes
// are: user's OK-long switcher, standby wake (returns s_prev), and auto
// deep-sleep. s_prev exists solely for the standby wake path.
// Persist the mode to land on after deep-sleep wake (pre-standby mode).
void app_shell_sleep_prepare(void);

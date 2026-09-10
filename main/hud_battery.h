// main/hud_battery.h —— classic battery glyph + percent, switcher only.
#pragma once

#include "lvgl.h"

// Mount the status cluster on the mode-switcher screen. No-op if already
// attached to the same parent. Safe to call again after detach.
void hud_battery_attach(lv_obj_t *parent);

// Stop the poll timer and drop widgets. Call before deleting the switcher
// screen. Idempotent.
void hud_battery_detach(void);

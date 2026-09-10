// main/demo_deskpet.h —— standalone page interface (was demo.h upstream).
#pragma once

#include "bsp_button.h"

void demo_deskpet_enter(void);
void demo_deskpet_exit(void);
void demo_deskpet_key(bsp_btn_t btn, bsp_btn_ev_t ev);
// Device audio task and simulator acceptance both consume the current scene.
int demo_deskpet_audio_scene(void);   // CT_SCENE_x for the BGM task
#ifndef ESP_PLATFORM
// Simulator-only acceptance hooks; excluded from device firmware.
// QA cheat (sim script "warp:10"): jump floor for boss shots. No other effects.
void demo_deskpet_debug_floor(unsigned floor);
void demo_deskpet_debug_art(const char *scene);
#endif

// main/audio_task.h —— device audio frontend (BGM task + persisted volumes).
#pragma once

#include <stdint.h>

void audio_task_start(void);                       // app_main calls once
void audio_set_volumes(uint8_t bgm, uint8_t se);   // persists to NVS
void audio_get_volumes(uint8_t *bgm, uint8_t *se);

// Mute preferences (settings page), 1 = muted. mute_all silences everything;
// game / pomo mute their own mode; other mutes every remaining scope (shell,
// clicker, standby, agent cues). Default: mute_all=1 (silent out of the box).
typedef struct {
    uint8_t mute_all, mute_game, mute_pomo, mute_other;
} audio_prefs_t;
void audio_prefs_get(audio_prefs_t *out);
void audio_prefs_set(const audio_prefs_t *p);      // persists to NVS

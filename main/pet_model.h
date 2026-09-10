// main/pet_model.h —— DeskPet mood core: pure logic, no ESP-IDF/LVGL.
// Mood is a pure function of triggers; rendering maps the result to sprites.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PET_MODEL_VERSION 1u
#define PET_GROWTH_PER_POMO 1u
#define PET_IDLE_SLEEP_MS (5u * 60u * 1000u)  // no poke for 5 min -> SLEEP

typedef enum {
    PET_MOOD_NORMAL = 0,
    PET_MOOD_HAPPY,
    PET_MOOD_SAD,
    PET_MOOD_BUSY,
    PET_MOOD_CELEBRATE,
    PET_MOOD_FOCUS,
    PET_MOOD_LOWBAT,
    PET_MOOD_SLEEP,
    PET_MOOD_COUNT,
} pet_mood_t;

typedef struct {
    bool sleeping;          // device entering light sleep
    bool battery_low;       // SOC < 20%
    bool pomo_running;      // pomodoro focus running
    bool pomo_celebrate;    // celebration window after pomo/build win
    bool pc_busy;           // BLE/PC reports RUN (phase 2; manual for now)
    bool bad_news;          // build fail / pomo abandon (sticky, cleared on poke)
    bool poked;             // user poked this tick -> HAPPY pulse
    uint64_t idle_ms;       // ms since last key
} pet_triggers_t;

typedef struct {
    uint16_t version;
    uint32_t growth;        // lifetime completed pomodoros
    uint32_t poke_count;
    uint8_t muted;
    uint8_t reserved[3];
} pet_save_t;               // < 16 bytes, NVS-backed

void pet_save_defaults(pet_save_t *s);
pet_mood_t pet_mood_resolve(const pet_triggers_t *t);
const char *pet_mood_name_en(pet_mood_t m);

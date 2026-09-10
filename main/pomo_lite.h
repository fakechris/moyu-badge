// main/pomo_lite.h —— minimal pomodoro: IDLE/RUNNING/PAUSED/REWARD/BREAK.
// Monotonic ms in, event out. No RTC, no LVGL, host-testable.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define POMO_LITE_VERSION 1u
#define POMO_FOCUS_MS (25u * 60u * 1000u)
#define POMO_BREAK_MS (5u * 60u * 1000u)
#define POMO_REWARD_MS 3000u

typedef enum {
    POMO_IDLE = 0, POMO_RUNNING, POMO_PAUSED, POMO_REWARD, POMO_BREAK,
} pomo_state_t;

typedef enum {
    POMO_EV_NONE = 0, POMO_EV_FOCUS_DONE, POMO_EV_REWARD_DONE, POMO_EV_BREAK_DONE,
} pomo_event_t;

typedef struct {
    uint16_t version;
    pomo_state_t state;
    uint32_t remaining_ms;
    uint32_t completed;      // lifetime focus sessions
    uint64_t deadline_ms;    // boot-relative; meaningless after reboot (see pomo_rebase)
} pomo_t;

void pomo_defaults(pomo_t *p);
void pomo_start(pomo_t *p, uint64_t now_ms);
void pomo_pause(pomo_t *p, uint64_t now_ms);
void pomo_resume(pomo_t *p, uint64_t now_ms);
void pomo_abandon(pomo_t *p);
pomo_event_t pomo_tick(pomo_t *p, uint64_t now_ms);
// After a save load: a RUNNING session becomes PAUSED at its last known
// remaining time; REWARD/BREAK fall back to IDLE (their deadlines are void).
void pomo_rebase(pomo_t *p);

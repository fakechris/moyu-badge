// main/pomo_lite.c
#include "pomo_lite.h"

void pomo_defaults(pomo_t *p)
{
    p->version = POMO_LITE_VERSION;
    p->state = POMO_IDLE;
    p->remaining_ms = POMO_FOCUS_MS;
    p->completed = 0;
    p->deadline_ms = 0;
}

void pomo_start(pomo_t *p, uint64_t now_ms)
{
    p->state = POMO_RUNNING;
    p->remaining_ms = POMO_FOCUS_MS;
    p->deadline_ms = now_ms + POMO_FOCUS_MS;
}

void pomo_pause(pomo_t *p, uint64_t now_ms)
{
    if (p->state != POMO_RUNNING) return;
    p->remaining_ms = (p->deadline_ms > now_ms) ? (uint32_t)(p->deadline_ms - now_ms) : 0;
    p->state = POMO_PAUSED;
}

void pomo_resume(pomo_t *p, uint64_t now_ms)
{
    if (p->state != POMO_PAUSED) return;
    p->state = POMO_RUNNING;
    p->deadline_ms = now_ms + p->remaining_ms;
}

void pomo_abandon(pomo_t *p)
{
    if (p->state == POMO_RUNNING || p->state == POMO_PAUSED) {
        p->state = POMO_IDLE;
        p->remaining_ms = POMO_FOCUS_MS;
    }
}

void pomo_rebase(pomo_t *p)
{
    if (p->state == POMO_RUNNING) p->state = POMO_PAUSED;
    else if (p->state == POMO_REWARD || p->state == POMO_BREAK) {
        p->state = POMO_IDLE;
        p->remaining_ms = POMO_FOCUS_MS;
    }
    p->deadline_ms = 0;
}

pomo_event_t pomo_tick(pomo_t *p, uint64_t now_ms)
{
    if (p->state != POMO_RUNNING && p->state != POMO_REWARD && p->state != POMO_BREAK)
        return POMO_EV_NONE;
    if (p->state != POMO_REWARD)   // live countdown for the display and for saves
        p->remaining_ms = (p->deadline_ms > now_ms) ? (uint32_t)(p->deadline_ms - now_ms) : 0;
    if (now_ms < p->deadline_ms) return POMO_EV_NONE;
    if (p->state == POMO_RUNNING) {
        p->state = POMO_REWARD;
        p->deadline_ms = now_ms + POMO_REWARD_MS;
        p->completed++;
        return POMO_EV_FOCUS_DONE;
    }
    if (p->state == POMO_REWARD) {
        p->state = POMO_BREAK;
        p->deadline_ms = now_ms + POMO_BREAK_MS;
        p->remaining_ms = POMO_BREAK_MS;
        return POMO_EV_REWARD_DONE;
    }
    p->state = POMO_IDLE;
    p->remaining_ms = POMO_FOCUS_MS;
    return POMO_EV_BREAK_DONE;
}

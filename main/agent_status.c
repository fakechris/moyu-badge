// main/agent_status.c
#include "agent_status.h"

#include <string.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
static portMUX_TYPE s_slots_lock = portMUX_INITIALIZER_UNLOCKED;
#define SLOTS_LOCK() portENTER_CRITICAL(&s_slots_lock)
#define SLOTS_UNLOCK() portEXIT_CRITICAL(&s_slots_lock)
#else
#define SLOTS_LOCK() ((void)0)
#define SLOTS_UNLOCK() ((void)0)
#endif

static agent_slot_t s_slots[AGENT_SLOTS];

static void copy_fixed(char *dst, size_t n, const char *src)
{
    if (!src) src = "";
    size_t i = 0;
    for (; i + 1 < n && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

void agent_status_set(uint8_t slot, agent_source_t source, agent_state_t state,
                      uint8_t progress, const char *name, const char *text,
                      uint64_t now_ms)
{
    if (slot >= AGENT_SLOTS || state >= AST_COUNT || source > ASRC_GENERIC) return;
    if (progress > 100 && progress != 255) progress = 255;
    SLOTS_LOCK();
    agent_slot_t *s = &s_slots[slot];
    s->used = 1;
    s->source = source;
    s->state = state;
    s->progress = progress;
    copy_fixed(s->name, sizeof(s->name), name);
    copy_fixed(s->text, sizeof(s->text), text);
    s->updated_ms = now_ms;
    SLOTS_UNLOCK();
}

void agent_status_clear(uint8_t slot)
{
    if (slot >= AGENT_SLOTS) return;
    SLOTS_LOCK();
    memset(&s_slots[slot], 0, sizeof(s_slots[slot]));
    SLOTS_UNLOCK();
}

static int prio(agent_state_t st)
{
    switch (st) {
    case AST_NEEDS_YOU: return 5;
    case AST_FAILED: return 4;
    case AST_WORKING: return 3;
    case AST_REVIEW: return 2;
    case AST_CELEBRATE: return 1;
    default: return 0;
    }
}

int agent_status_top(void)
{
    int best = -1, best_p = 0;
    uint64_t best_t = 0;
    SLOTS_LOCK();
    for (int i = 0; i < (int)AGENT_SLOTS; i++) {
        if (!s_slots[i].used || s_slots[i].state == AST_IDLE) continue;
        int p = prio(s_slots[i].state);
        if (p > best_p || (p == best_p && s_slots[i].updated_ms >= best_t)) {
            best = i;
            best_p = p;
            best_t = s_slots[i].updated_ms;
        }
    }
    SLOTS_UNLOCK();
    return best;
}

int agent_status_next(int cur)
{
    SLOTS_LOCK();
    for (int k = 1; k <= (int)AGENT_SLOTS; k++) {
        int i = (cur + k) % (int)AGENT_SLOTS;
        if (s_slots[i].used && s_slots[i].state != AST_IDLE) {
            SLOTS_UNLOCK();
            return i;
        }
    }
    SLOTS_UNLOCK();
    return cur;
}

bool agent_status_get_copy(uint8_t slot, agent_slot_t *out)
{
    if (slot >= AGENT_SLOTS || !out) return false;
    SLOTS_LOCK();
    *out = s_slots[slot];
    SLOTS_UNLOCK();
    return true;
}

const char *agent_state_name_en(agent_state_t state)
{
    switch (state) {
    case AST_WORKING: return "working";
    case AST_NEEDS_YOU: return "needs-you";
    case AST_REVIEW: return "review";
    case AST_FAILED: return "failed";
    case AST_CELEBRATE: return "done!";
    default: return "idle";
    }
}

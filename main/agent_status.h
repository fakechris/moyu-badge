// main/agent_status.h —— multi-agent coding status store (pure logic).
// Unified 6-state model covering codex hatch-pet (9 states), dsh obvious-grid
// and opencode sessions. N=4 slots, priority-ordered. No BLE/LVGL here;
// transport lives in agent_status_gatts.c, UI in mode_standby.c.
//
// Source mapping (sender side implements this table):
//   codex: idle->IDLE, running/running-right/running-left->WORKING,
//          waiting->NEEDS_YOU, review->REVIEW, failed->FAILED,
//          waving/jumping->CELEBRATE
//   dsh:   running->WORKING, approval-wait->NEEDS_YOU, turn-end->REVIEW,
//          error->FAILED, idle->IDLE
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define AGENT_SLOTS 4u
#define AGENT_NAME_LEN 16u
#define AGENT_TEXT_LEN 32u

typedef enum {
    AST_IDLE = 0,   // nothing to show (slot hides)
    AST_WORKING,    // running / thinking / typing
    AST_NEEDS_YOU,  // approval / input blocked (highest attention)
    AST_REVIEW,     // turn done, ready for review
    AST_FAILED,     // error / failed
    AST_CELEBRATE,  // done-acknowledged (waving/jumping)
    AST_COUNT,
} agent_state_t;

typedef enum {
    ASRC_CODEX = 0,
    ASRC_DSH,
    ASRC_OPENCODE,
    ASRC_GENERIC,
} agent_source_t;

typedef struct {
    uint8_t used;
    agent_source_t source;
    agent_state_t state;
    uint8_t progress;  // 0-100, 255 = unknown
    char name[AGENT_NAME_LEN];
    char text[AGENT_TEXT_LEN];
    uint64_t updated_ms;
} agent_slot_t;

void agent_status_set(uint8_t slot, agent_source_t source, agent_state_t state,
                      uint8_t progress, const char *name, const char *text,
                      uint64_t now_ms);
void agent_status_clear(uint8_t slot);
// Top slot by priority (NEEDS_YOU > FAILED > WORKING > REVIEW > CELEBRATE;
// IDLE/empty never top). Returns -1 when nothing to show.
int agent_status_top(void);
// Browse order for UP/DOWN in standby: next non-idle slot after cur.
int agent_status_next(int cur);
// Copy a coherent snapshot. BLE callbacks update slots outside the LVGL task,
// so callers must never retain a pointer into the shared store.
bool agent_status_get_copy(uint8_t slot, agent_slot_t *out);
const char *agent_state_name_en(agent_state_t state);

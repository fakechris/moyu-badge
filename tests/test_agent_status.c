// Pure host regression tests for the multi-agent priority/snapshot store.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "agent_status.h"

static void clear_all(void)
{
    for (uint8_t i = 0; i < AGENT_SLOTS; i++) agent_status_clear(i);
}

static void test_priority_and_recency(void)
{
    clear_all();
    assert(agent_status_top() == -1);
    agent_status_set(0, ASRC_CODEX, AST_WORKING, 25, "codex", "build", 10);
    agent_status_set(1, ASRC_DSH, AST_REVIEW, 100, "dsh", "review", 20);
    assert(agent_status_top() == 0);
    agent_status_set(2, ASRC_OPENCODE, AST_NEEDS_YOU, 255, "open", "input", 30);
    assert(agent_status_top() == 2);
    agent_status_set(3, ASRC_GENERIC, AST_NEEDS_YOU, 50, "other", "approval", 40);
    assert(agent_status_top() == 3);
    printf("agent_status_priority OK\n");
}

static void test_browse_and_snapshot(void)
{
    agent_slot_t copy;
    assert(agent_status_next(0) == 1);
    assert(agent_status_get_copy(2, &copy));
    assert(copy.used && copy.state == AST_NEEDS_YOU);
    assert(strcmp(copy.name, "open") == 0);
    copy.state = AST_FAILED;
    agent_slot_t again;
    assert(agent_status_get_copy(2, &again));
    assert(again.state == AST_NEEDS_YOU);  // snapshot is not shared storage
    assert(!agent_status_get_copy(AGENT_SLOTS, &again));
    assert(!agent_status_get_copy(0, NULL));
    agent_status_set(0, ASRC_CODEX, AST_WORKING, 222, "codex", "unknown", 50);
    assert(agent_status_get_copy(0, &again) && again.progress == 255);
    printf("agent_status_snapshot OK\n");
}

int main(void)
{
    test_priority_and_recency();
    test_browse_and_snapshot();
    printf("ALL AGENT STATUS TESTS PASS\n");
    return 0;
}

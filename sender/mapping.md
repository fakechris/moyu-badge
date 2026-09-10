# 源始状态映射(历史参考)

> 状态采集已委托 planofplan(`GET /api/agent-status`);现行映射表与精度
> 分级由那边维护:`../planofplan/docs/agent-status-api.md`。本文件保留
> 2026-09-04 首次调研的原始映射,作为语义出处。

# Per-source state mapping to DeskPet 6-state model.
# Firmware enum: 0 idle / 1 working / 2 needs-you / 3 review / 4 failed / 5 celebrate.

## codex hatch-pet (9 states)
idle -> 0, running / running-right / running-left -> 1, waiting -> 2,
review -> 3, failed -> 4, waving / jumping -> 5.

## dsh obvious-grid
running -> 1, waiting-on-you (blue) -> 2, turn-end -> 3, error (red) -> 4,
idle (green) -> hide (report 0 = slot clears).

## opencode serve (/session + /session/status)
status busy/retry -> 1; permission pending (non-empty permission list) -> 2;
recently idle with new messages since last poll -> 3; error status -> 4;
idle + stale -> hide.

## claude hooks
UserPromptSubmit / PreToolUse -> 1; permission_prompt Notification -> 2;
agent_needs_input -> 2; agent_completed / Stop -> 3; StopFailure / error -> 4;
SessionEnd -> hide. Text = cwd basename or last prompt snippet (<=32 chars,
ASCII preferred; CJK beyond the device font renders blank).

## codex CLI (~/.codex/sessions)
meta status active + mtime < 120s -> 1; failed -> 4; completed + mtime < 10min -> 3;
suspended/old -> hide. Title from meta, truncated.

## kimi (store probe + process)
session updatedAt < 120s with running process -> 1; ApprovalRequest event -> 2;
else hide. Best-effort: exact states need ACP hookup (future).

## zcode / amp / droid / grok
- zcode: config/transcript probe + process -> 1 while active turn (heuristic).
- amp: `amp` process alive -> 1 (running only; no approval visibility).
- droid: hooks state files (same schema as claude) -> exact; else process heuristic.
- grok: `grok sessions list` + transcript mtime -> 1/3 by recency.

## agy
STUB. Process-name scan only. Tell us what agy is (binary name, session store)
and it gets a real poller.

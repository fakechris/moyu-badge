#!/usr/bin/env bash
# hooks/install-claude.sh — append deskpet writers to ~/.claude/settings.json.
# Events: UserPromptSubmit->working, Notification(permission/agent_needs_input)->needs,
# agent_completed/Stop->review, StopFailure->failed, SessionEnd->hide.
set -euo pipefail
HOOK=~/.config/deskpet-sender/deskpet-hook.sh
mkdir -p ~/.config/deskpet-sender
cat > "$HOOK" << 'EOF'
#!/usr/bin/env bash
# stdin: hook JSON. $1: state to record (working/needs/review/failed/done/idle)
STATE="${1:-working}"
IN=$(cat)
SRC="claude"
CWD=$(printf '%s' "$IN" | python3 -c "import json,sys; print(json.load(sys.stdin).get('cwd','') or '')" 2>/dev/null || echo "")
TEXT=$(printf '%s' "$IN" | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('last_assistant_message',d.get('message','')) or '')" 2>/dev/null | head -c 32 || echo "")
KEY=$(printf '%s' "$CWD" | cksum | awk '{print $1}')
OUT=~/.config/deskpet-sender/claude-"$KEY".json
if [ "$STATE" = "idle" ]; then rm -f "$OUT"; exit 0; fi
python3 -c "import json,time; print(json.dumps({'state':'$STATE','cwd':'''$CWD''','text':'''$TEXT''','time':time.time()}))" > "$OUT"
EOF
chmod +x "$HOOK"
echo "hook installed at $HOOK"
echo 'Add to ~/.claude/settings.json hooks:'
echo "{\"UserPromptSubmit\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"$HOOK working\"}]}],\"Notification\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"$HOOK needs\"}]}],\"Stop\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"$HOOK review\"}]}],\"StopFailure\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"$HOOK failed\"}]}],\"SessionEnd\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"$HOOK idle\"}]}]}"

#!/usr/bin/env bash
# Product gate: source integrity, host logic, headless UI journeys, firmware.
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
cd "$project_dir"

python3 ${REPO_PLAYBOOK:-../moyu-playbook}/tools/pixelpost.py --check
cmake -S sim -B sim/build
cmake --build sim/build -j4

capture_dir="$(mktemp -d "${TMPDIR:-/tmp}/deskpet-product.XXXXXX")"
run_shot() {
    local name="$1"
    local script="$2"
    local state_dir="$capture_dir/state-$name"
    mkdir -p "$state_dir"
    SDL_VIDEODRIVER=dummy TMPDIR="$state_dir" \
        sim/build/deskpet-sim --script "$script" \
        --shot "$capture_dir/$name.rgb565" >/dev/null
    local bytes
    bytes="$(wc -c < "$capture_dir/$name.rgb565" | tr -d ' ')"
    if [[ "$bytes" != "153600" ]]; then
        echo "FAIL $name snapshot is $bytes bytes"
        exit 1
    fi
}

# Main game, all five job animation packs, all biome families, and event art.
run_shot town "art:town wait:100"
for job in 0 1 2 3 4; do
    run_shot "job${job}-walk-a" "art:job${job} art:walk wait:40"
    run_shot "job${job}-walk-b" "art:job${job} art:walk wait:520"
    run_shot "job${job}-attack" "art:job${job} art:attack wait:320"
    run_shot "job${job}-ult" "art:job${job} art:ult wait:40"
done
for floor in 1 21 41 61 81; do
    run_shot "floor${floor}" "floor:${floor} art:walk wait:40"
done
run_shot mimic-closed "art:mimic wait:40"
run_shot mimic-open "art:mimic wait:900"
run_shot rare "art:rare wait:40"
run_shot cursed-gate "art:gate wait:40"
run_shot starfall "art:starfall wait:40"
run_shot return-town "art:walk down down down ok wait:40"

# Shell modes and every coding-status state remain renderable headlessly.
run_shot switcher "okl wait:100"
run_shot pomo "okl down ok wait:100"
run_shot clicker "okl down down ok wait:100"
run_shot standby-idle "okl down down down down ok wait:100"
for state in working needs review failed done; do
    run_shot "agent-$state" \
        "okl down down down down ok agent 0 codex $state 70 codex product_check wait:1100"
done

# Button journeys: timer lifecycle, three HID actions, standby browsing, and
# the game's explicit return-to-town row. Simulator logs make HID delivery
# count observable without pretending that a host Bluetooth link exists.
flow_state="$capture_dir/state-button-flows"
mkdir -p "$flow_state"
SDL_VIDEODRIVER=dummy TMPDIR="$flow_state" sim/build/deskpet-sim \
    --script "okl down ok ok wait:20 ok ok down wait:20" >/dev/null
clicker_log="$capture_dir/clicker-flow.log"
SDL_VIDEODRIVER=dummy TMPDIR="$flow_state" sim/build/deskpet-sim \
    --script "okl down ok up down ok wait:20" >"$clicker_log"
if ! grep -q "#3" "$clicker_log"; then
    echo "FAIL clicker did not deliver all three HID reports"
    exit 1
fi
SDL_VIDEODRIVER=dummy TMPDIR="$flow_state" sim/build/deskpet-sim \
    --script "okl down ok agent 0 codex working 20 codex build agent 1 dsh needs - dsh approval down up wait:20" \
    >/dev/null

# Regression: a prior persisted non-game mode must not crash art/floor QA hooks.
persist_dir="$capture_dir/state-persisted-mode"
mkdir -p "$persist_dir"
SDL_VIDEODRIVER=dummy TMPDIR="$persist_dir" sim/build/deskpet-sim \
    --script "okl down ok wait:20" >/dev/null
SDL_VIDEODRIVER=dummy TMPDIR="$persist_dir" sim/build/deskpet-sim \
    --script "floor:41 art:walk wait:40" \
    --shot "$capture_dir/persisted-mode-art.rgb565" >/dev/null

for job in 0 1 2 3 4; do
    if cmp -s "$capture_dir/job${job}-walk-a.rgb565" \
              "$capture_dir/job${job}-walk-b.rgb565"; then
        echo "FAIL job $job walk animation did not advance"
        exit 1
    fi
done
if cmp -s "$capture_dir/mimic-closed.rgb565" "$capture_dir/mimic-open.rgb565"; then
    echo "FAIL mimic did not reveal before combat"
    exit 1
fi

if ! command -v idf.py >/dev/null 2>&1; then
    echo "FAIL idf.py is not available; source the ESP-IDF export script"
    exit 1
fi
# ESP-IDF computes PROJECT_VER during configure. An incremental build alone can
# otherwise emit a fresh image carrying yesterday's Git description.
idf.py reconfigure
idf.py build
python3 tools/budget.py

echo "PRODUCT_CHECK_OK"
echo "screenshots: $capture_dir"

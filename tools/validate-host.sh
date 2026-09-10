#!/usr/bin/env bash
# Host gates: model tests, chiptune/anim/agent tests, balance sim, python asset/sender tests.
# No hardware, no ESP-IDF. Run from the repo root.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=${TMPDIR:-/tmp}/deskpet-host
mkdir -p "$OUT"
CC=${CC:-cc}
FLAGS="-std=c11 -Wall -Wextra -Werror -Imain"
echo "== model tests"
$CC $FLAGS tests/test_deskpet_models.c main/pet_model.c main/dungeon_model.c main/pomo_lite.c main/deskpet_i18n.c -o "$OUT/models"
"$OUT/models" | tail -3
echo "== vocab FSRS"
$CC $FLAGS tests/test_vocab.c main/vocab_model.c -o "$OUT/vocab"
"$OUT/vocab" | tail -1
echo "== vocab deck data"
$CC $FLAGS tests/test_vocab_data.c main/vocab_model.c -o "$OUT/vocab_data"
"$OUT/vocab_data" | tail -1
echo "== agent status"
$CC $FLAGS tests/test_agent_status.c main/agent_status.c -o "$OUT/agent"
"$OUT/agent" | tail -1
echo "== chiptune"
$CC $FLAGS tests/test_chiptune.c main/chiptune.c main/chiptune_songs.c -lm -o "$OUT/chip"
"$OUT/chip" | tail -1
echo "== sprite anim"
$CC $FLAGS -Isim/third_party/lvgl -Isim -DLV_CONF_INCLUDE_SIMPLE tests/test_sprite_anim.c main/sprite_anim.c -o "$OUT/anim"
"$OUT/anim" | tail -1
echo "== balance sim"
# two-repo split: research tools live in moyu-playbook
PB=${REPO_PLAYBOOK:-../moyu-playbook}
$CC -std=c11 -O2 -Imain $PB/tools/balance_sim.c main/dungeon_model.c -o "$OUT/balance"
"$OUT/balance" | tail -1
echo "== python"
python3 tests/test_asset_packing.py
python3 tests/test_sender.py | tail -1
echo "== design doc"
python3 tools/check_design_doc.py
echo "HOST GATES PASS"

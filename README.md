# DeskPet — idle dungeon for AI Passport

A pocket idle labyrinth: the pet climbs a 100-floor tower on its own, you
make a few heavy choices (job, skill, room card, retreat). Same device also
has a pomodoro timer, a clicker / presenter, vocabulary review, and a clock
standby. Three keys. Chinese and English.

## Build

Needs ESP-IDF 5.5.3 and the AI Passport board support package.

```bash
source ~/esp/esp-idf-v5.5.3/export.sh
# BSP defaults to ../my-ai-passport/ai-passport/components (override with DESKPET_BSP_DIR)
idf.py build
idf.py merge-bin -o build/FoloToy-AI-Passport-full.bin
```

`build/FoloToy-AI-Passport-full.bin` is the merged 8 MB image for the community
installer (flash from `0x0`). Day-to-day device updates can write only the
factory app with `tools/flash.sh` (never `erase-flash`).

Host checks (no hardware):

```bash
tools/validate-host.sh
python3 tools/budget.py
```

## Layout

```text
main/     firmware, public sprites, fonts, word list
tools/    flash / host gates / size budget
sim/      desktop simulator (keyboard = three keys)
tests/    host model tests
docs/     SYSTEM_DESIGN.md (balance knobs)
```

## Safety

The image keeps the device identity partition and the factory Recovery slot
empty. Do not `erase-flash`. Holding UP for 5 seconds at boot still enters
the device's permanent Recovery.

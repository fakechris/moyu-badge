#!/usr/bin/env python3
"""Budget gates (DESIGN 1): factory size + save size + art size.

Usage:
    python3 tools/budget.py [--bin build/deskpet-game.bin]

Gates (fail nonzero):
  1. app .bin < 0x2E0000 (3MB factory minus 128KB margin)
  2. packed embedded art (main/sprites) < fixed 1.6MB production ceiling
  3. host model tests pass (includes save <2KB assert)
"""
import subprocess
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FACTORY = 0x300000
APP_CAP = FACTORY - 0x20000
ART_CAP = 1600000  # fixed packed-art ceiling; APP_CAP remains the shipping gate
SAVE_CAP = 2048


def main() -> int:
    fails = 0
    # One firmware image embeds shared art + exactly one hero flavor
    # (main/sprites/oc or generic); count the heaviest flavor present.
    sprites = ROOT / "main" / "sprites"
    suffixes = {".argb8888", ".i4", ".spr4"}
    art = sum(p.stat().st_size for p in sprites.iterdir()
              if p.is_file() and p.suffix in suffixes)
    for flavor in ("oc", "generic"):
        directory = sprites / flavor
        if directory.is_dir():
            flavor_bytes = sum(p.stat().st_size for p in directory.iterdir()
                               if p.suffix in suffixes)
            if flavor_bytes > 0:
                art += flavor_bytes
                break
    print(f"art: {art} / {ART_CAP}")
    if art > ART_CAP:
        print("FAIL art over cap");
        fails += 1
    bins = [ROOT / "build" / "deskpet-game.bin"]
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=str(bins[0]))
    a = ap.parse_args()
    bp = Path(a.bin)
    if bp.is_file():
        n = bp.stat().st_size
        print(f"app: {n} / {APP_CAP} (factory {FACTORY})")
        if n > APP_CAP:
            print("FAIL app over cap");
            fails += 1
        # A failed incremental IDF build leaves the previous .bin in place.
        # Size alone would then report a false green for the current source.
        ver = subprocess.run(
            ["git", "describe", "--always", "--dirty"], cwd=ROOT,
            capture_output=True, text=True, check=True).stdout.strip()
        if ver.encode() + b"\0" not in bp.read_bytes():
            print(f"FAIL app binary is stale (expected version {ver})")
            fails += 1
        # `git describe --dirty` stays identical across multiple edits in one
        # worktree. Also require the image to postdate every firmware input.
        inputs = [p for p in (ROOT / "main").rglob("*") if p.is_file()]
        inputs += [ROOT / name for name in
                   ("CMakeLists.txt", "partitions.csv", "sdkconfig", "sdkconfig.defaults")
                   if (ROOT / name).is_file()]
        newer = [p for p in inputs if p.stat().st_mtime > bp.stat().st_mtime]
        if newer:
            print(f"FAIL app binary predates {len(newer)} firmware input(s), e.g. {newer[0].relative_to(ROOT)}")
            fails += 1
    else:
        print(f"app: no {bp} (idf.py build first), skipping app gate")
    r = subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Imain",
         "tests/test_deskpet_models.c", "main/pet_model.c", "main/dungeon_model.c",
         "main/pomo_lite.c", "main/deskpet_i18n.c", "-o", "/tmp/budget_models"],
        cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        print("FAIL model tests build:\n" + r.stderr[:2000]);
        return 1
    r = subprocess.run(["/tmp/budget_models"], capture_output=True, text=True)
    print(r.stdout.strip().splitlines()[-3:])
    if r.returncode != 0 or "ALL DESKPET MODEL TESTS PASS" not in r.stdout:
        print("FAIL model tests:\n" + r.stdout[-2000:] + r.stderr[-500:]);
        fails += 1
    # Gate 3.5: chiptune render tests (pure engine, deterministic).
    r = subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Imain",
         "tests/test_chiptune.c", "main/chiptune.c", "main/chiptune_songs.c",
         "-o", "/tmp/budget_chiptune"],
        cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        print("FAIL chiptune tests build:\n" + r.stderr[:2000])
        return 1
    r = subprocess.run(["/tmp/budget_chiptune"], capture_output=True, text=True)
    if r.returncode != 0 or "ALL CHIPTUNE TESTS PASS" not in r.stdout:
        print("FAIL chiptune tests:\n" + r.stdout[-1500:] + r.stderr[-500:])
        fails += 1
    else:
        print(r.stdout.strip().splitlines()[-1:])

    # Gate 3.6: coding-status snapshots and attention priority.
    r = subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Imain",
         "tests/test_agent_status.c", "main/agent_status.c",
         "-o", "/tmp/budget_agent_status"],
        cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        print("FAIL agent status tests build:\n" + r.stderr[:2000])
        return 1
    r = subprocess.run(["/tmp/budget_agent_status"], capture_output=True, text=True)
    if r.returncode != 0 or "ALL AGENT STATUS TESTS PASS" not in r.stdout:
        print("FAIL agent status tests:\n" + r.stdout[-1500:] + r.stderr[-500:])
        fails += 1
    else:
        print(r.stdout.strip().splitlines()[-1:])

    # Gate 3.7: sender packet safety, slot clearing, and urgent-task eviction.
    r = subprocess.run([sys.executable, "tests/test_sender.py"], cwd=ROOT,
                       capture_output=True, text=True)
    if r.returncode != 0 or "ALL SENDER TESTS PASS" not in r.stdout:
        print("FAIL sender tests:\n" + r.stdout[-1500:] + r.stderr[-500:])
        fails += 1
    else:
        print(r.stdout.strip().splitlines()[-1:])

    # Gate 3.8: backgrounds use I4; transformed art uses safe BGRA ARGB8888.
    r = subprocess.run([sys.executable, "tests/test_asset_packing.py"], cwd=ROOT,
                       capture_output=True, text=True)
    if r.returncode != 0 or "ALL STATIC ASSET FORMATS PASS" not in r.stdout:
        print("FAIL static asset contract:\n" + r.stdout[-1500:] + r.stderr[-1000:])
        fails += 1
    else:
        print(r.stdout.strip().splitlines()[-1:])

    # Gate 3.9: SP4 RGBA palettes must be converted to LVGL's BGRA memory order.
    r = subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
         "-DLV_CONF_INCLUDE_SIMPLE", "-Isim", "-Isim/third_party/lvgl",
         "-Isim/third_party/lvgl/src", "-Imain", "tests/test_sprite_anim.c",
         "main/sprite_anim.c", "-o", "/tmp/budget_sprite_anim"],
        cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        print("FAIL sprite animation tests build:\n" + r.stderr[:2000])
        return 1
    r = subprocess.run(["/tmp/budget_sprite_anim"], cwd=ROOT,
                       capture_output=True, text=True)
    if r.returncode != 0 or "ALL SPRITE ANIMATION COLORS PASS" not in r.stdout:
        print("FAIL sprite animation tests:\n" + r.stdout[-1500:] + r.stderr[-500:])
        fails += 1
    else:
        print(r.stdout.strip().splitlines()[-1:])

    # Gate 4: balance sim (bands + bots + soak). ~1-2 min, deterministic.
    r = subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Imain",
         os.environ.get("REPO_PLAYBOOK", str(Path(__file__).resolve().parent.parent.parent / "moyu-playbook"))
         + "/tools/balance_sim.c", "main/dungeon_model.c", "-o", "/tmp/budget_balance"],
        cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        print("FAIL balance build:\n" + r.stderr[:2000]);
        return 1
    r = subprocess.run(["/tmp/budget_balance"], capture_output=True, text=True, timeout=600)
    print(r.stdout.strip().splitlines()[-10:])
    if r.returncode != 0 or "BALANCE SIM PASS" not in r.stdout:
        print("FAIL balance sim:\n" + r.stdout[-2000:]);
        fails += 1
    print("BUDGET_OK" if fails == 0 else "BUDGET_FAILED")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())

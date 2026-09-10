#!/usr/bin/env python3
"""Structural contract for flash-efficient, LVGL-native static art.

Art layout (see tools/hero_flavors.py): shared world art sits in
main/sprites/, and each hero flavor set ("oc" = private, "generic" = public)
sits in main/sprites/<flavor>/ with identical stems. Exactly one flavor is
embedded per firmware image; every present flavor must be complete.
"""

import os
import re
import sys
from pathlib import Path

# two-repo split: the flavor contract module lives in the playbook repo
_here = Path(__file__).resolve().parent
os.environ.setdefault("DESKPET_ROOT", str(_here.parent))
for _cand in (_here.parent.parent / "moyu-playbook" / "tools", _here.parent / "tools"):
    if (_cand / "hero_flavors.py").exists():
        sys.path.insert(0, str(_cand))
        break
from hero_flavors import FLAVORS, ANIM_STEMS, SPRITES, is_flavor_stem

ROOT = Path(__file__).resolve().parents[1]

SP4_PACK_BYTES = 8 + 64 + 15 * (64 * 64 // 2)


def main() -> None:
    unsafe = sorted(
        p.name
        for p in SPRITES.iterdir()
        if p.is_file() and p.suffix in {".rgb565", ".i8"}
    )
    assert not unsafe, f"unsafe static format remains: {unsafe[:5]}"

    source = (ROOT / "main" / "sprite.c").read_text()
    entries = re.findall(
        r'\[(SPR_\w+)\]\s*=\s*(A888|I4)\("([\w]+)",\s*(\d+),\s*(\d+)\)',
        source,
    )
    assert entries, "sprite registry has no static assets"

    registered_shared = set()
    registered_flavor = set()
    for sprite_id, fmt, stem, width, height in entries:
        suffix = "argb8888" if fmt == "A888" else "i4"
        filename = f"{stem}.{suffix}"
        width, height = int(width), int(height)
        if is_flavor_stem(stem):
            registered_flavor.add(filename)
        else:
            registered_shared.add(filename)
        assert suffix == "i4" or fmt == "A888", sprite_id
        if stem.startswith("bg_"):
            assert fmt == "I4", f"{sprite_id}: full-screen background is not I4"
        else:
            assert fmt == "A888", f"{sprite_id}: transformed art must avoid indexed decoder"
        # Every registered file must exist exactly once: shared art at the
        # root, flavor art in every flavor directory that is present.
        locations = [SPRITES / filename] if not is_flavor_stem(stem) else [
            SPRITES / flavor / filename
            for flavor in FLAVORS
            if (SPRITES / flavor).is_dir()
        ]
        assert locations, f"{sprite_id}: no flavor directory present at all"
        for path in locations:
            assert path.is_file(), f"{sprite_id}: missing {path.relative_to(ROOT)}"
            expected = (64 + ((width + 1) // 2) * height
                        if fmt == "I4" else width * height * 4)
            assert path.stat().st_size == expected, (
                f"{sprite_id}: {path} is {path.stat().st_size}, expected {expected}"
            )

    shared = {p.name for p in SPRITES.iterdir()
              if p.is_file() and p.suffix in {".argb8888", ".i4"}}
    assert shared == registered_shared, (
        f"shared files and registry differ: extra={sorted(shared - registered_shared)}, "
        f"missing={sorted(registered_shared - shared)}"
    )

    for flavor in FLAVORS:
        directory = SPRITES / flavor
        if not directory.is_dir():
            continue
        static = {p.name for p in directory.iterdir()
                  if p.suffix in {".argb8888", ".i4"}}
        assert static == registered_flavor, (
            f"{flavor} files and registry differ: extra={sorted(static - registered_flavor)}, "
            f"missing={sorted(registered_flavor - static)}"
        )
        packs = {p.name for p in directory.iterdir() if p.suffix == ".spr4"}
        expected_packs = {f"{stem}.spr4" for stem in ANIM_STEMS}
        assert packs == expected_packs, (
            f"{flavor} animation packs differ: extra={sorted(packs - expected_packs)}, "
            f"missing={sorted(expected_packs - packs)}"
        )
        for name in packs:
            assert (directory / name).stat().st_size == SP4_PACK_BYTES, (
                f"{flavor}/{name}: bad SP4 pack size"
            )

    print(f"ALL STATIC ASSET FORMATS PASS ({len(entries)} sprites)")


if __name__ == "__main__":
    main()

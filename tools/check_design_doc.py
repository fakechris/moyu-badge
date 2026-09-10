#!/usr/bin/env python3
"""check_design_doc.py -- 系统设计正本门禁 (docs/SYSTEM_DESIGN.md).

Rules (all must hold, else HOST GATES fails):
  1. Every TUN_* knob defined in main/ appears in the doc's §9 parameter table.
     (改代码不改文档 = 门红 -- the whole point of this gate.)
  2. The doc's model-version header matches DUNGEON_MODEL_VERSION in code.
  3. Every gate named in the doc's §8 map exists as a section/print in
     balance_sim.c or benchmark.c or tests/ (light check by keyword).
"""
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# two-repo split: sim/benchmark sources live in the playbook repo
PLAYBOOK = Path(os.environ.get(
    "REPO_PLAYBOOK",
    Path(__file__).resolve().parent.parent.parent / "moyu-playbook",
))
if not PLAYBOOK.exists():
    PLAYBOOK = ROOT  # fallback: legacy single-repo layout
DOC = ROOT / "docs" / "SYSTEM_DESIGN.md"
fail = []

src_c = (ROOT / "main" / "dungeon_model.c").read_text()
src_h = (ROOT / "main" / "dungeon_model.h").read_text()
doc = DOC.read_text() if DOC.exists() else ""

# --- 1. knob coverage -------------------------------------------------------
knobs = set(re.findall(r"^#(?:define|ifndef)\s+(TUN_[A-Z0-9_]+)", src_c + src_h, re.M))
# knobs that live in other files on purpose (none today -- keep list empty)
allow_missing = set()
for k in sorted(knobs - allow_missing):
    if k not in doc:
        fail.append(f"knob {k} not documented in SYSTEM_DESIGN.md §9")

# --- 2. model version match -------------------------------------------------
m_code = re.search(r"#define\s+DUNGEON_MODEL_VERSION\s+(\d+)", src_h)
m_doc = re.search(r"model version (\d+)", doc)
if not m_code:
    fail.append("DUNGEON_MODEL_VERSION not found in dungeon_model.h")
elif not m_doc:
    fail.append("doc header missing 'model version N'")
elif m_code.group(1) != m_doc.group(1):
    fail.append(f"doc says model version {m_doc.group(1)}, code says {m_code.group(1)}"
                " -- update docs/SYSTEM_DESIGN.md header + §10")

# --- 3. gate-name presence in harness code ----------------------------------
sim = (PLAYBOOK / "tools" / "balance_sim.c").read_text() if (PLAYBOOK / "tools" / "balance_sim.c").exists() else ""
bench = (PLAYBOOK / "tools" / "benchmark.c").read_text() if (PLAYBOOK / "tools" / "benchmark.c").exists() else ""
for gate_kw, gate_name in [("G1", "G1 wall"), ("G4", "G4"), ("G6", "G6"),
                           ("treadmill", "treadmill"), ("prestige_loop", "prestige_loop")]:
    if gate_kw not in sim:
        fail.append(f"§8 documents gate '{gate_name}' but balance_sim.c has no '{gate_kw}'")
for sec_kw in ["新档曲线", "职业差异", "战斗张力", "压力曲线", "跑步机", "长线", "操作价值", "健康度"]:
    if sec_kw not in bench:
        fail.append(f"§8 documents benchmark section '{sec_kw}' not found in benchmark.c")

if fail:
    print("DESIGN DOC GATE FAIL:")
    for f in fail:
        print("  -", f)
    print("fix: update docs/SYSTEM_DESIGN.md (§9 knobs / header version / §8 gates)")
    sys.exit(1)
print(f"DESIGN DOC GATE PASS ({len(knobs)} knobs documented, model version {m_code.group(1)})")

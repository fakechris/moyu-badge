# AGENTS.md —— moyu-badge 仓库契约（发版仓）

> 两仓规范（2026-09-09 落地迁移，原则见 memory/AGENTS.md §1）。
> 本仓 = **发版仓**：只存发版本必须的东西。研究、方法论、制作管线一律在 **../moyu-playbook**。

## 本仓允许的内容

| 路径 | 内容 |
|---|---|
| `main/` | 固件代码 + `main/sprites/generic` 打包公版素材 + 字体/词表生成产物 |
| `tests/` + `sim/` | 固件回归门（host 可跑） |
| `tools/` | 仅发布工具：validate-host.sh / flash.sh / product_check.sh / budget.py / import_anki.py / gen_zh_subset.py / pack_static_assets.py / check_design_doc.py |
| `docs/` | 仅 SYSTEM_DESIGN.md（TUN 门禁目标）+ RELEASE_CANDIDATES.md（晋升台账） |

## 本仓禁止的内容（放 playbook）

研究文档、迭代日志、指标体系、scorecard/benchmark/diag/autotune 等研究工具、
素材制作管线与**任何中间素材**（raw/review/trial）、OC 私有源素材（本就 gitignored）。
判定口诀：**"发版本需要它吗？" 不是 → playbook。**

## 跨仓约定（改这些文件前必读）

- `tools/validate-host.sh` 的 balance 门与 `tools/check_design_doc.py` 从
  `../moyu-playbook/tools/` 读源（`REPO_PLAYBOOK` 可覆盖）。
- `tests/test_asset_packing.py` 经 `DESKPET_ROOT` 读 playbook 的 `hero_flavors.py`。
- playbook 侧 `scorecard.py` 用 `BADGE_DIR` 指回本仓任一检出。
- 固件构建：`PATH="$HOME/.espressif/python_env/idf5.5_py3.14_env/bin:$PATH"` 先于 source
  `~/esp/esp-idf-v5.5.3/export.sh`（机器上有多个 python，顺序错了 venv 找不到）。

## 分支与刷机

- `main` = 发版线。自治迭代循环在 `../moyu-badge-autogoal`（worktree，分支 `autogoal`），
  其成果经 `autogoal-rc` 分支晋升，**本仓只 `git merge autogoal-rc`**。
- 刷机由本仓检出的操作者执行；循环无串口权限。

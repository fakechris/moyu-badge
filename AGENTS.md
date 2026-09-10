# AGENTS.md —— moyu-badge 仓库契约（发版仓）

本仓 = **发版仓**：只放发版本必须的东西。
研究、方法论、制作管线、中间素材一律在 **`../moyu-playbook`**。
判定口诀：**「发版本需要它吗？」不是 → playbook。**

本地工作原则（循环、跨仓命令）在 gitignored 的 `memory/AGENTS.md`。

## 本仓允许的内容

| 路径 | 内容 |
|---|---|
| `main/` | 固件代码 + `main/sprites/generic` 打包公版素材 + 字体/词表生成产物 |
| `tests/` + `sim/` | 固件回归门（host 可跑）；sim 自包含 vendored LVGL |
| `sender/` | agent 状态 BLE 推送客户端 |
| `tools/` | 仅发布工具：`validate-host.sh`、`flash.sh`、`product_check.sh`、`budget.py`、`check_design_doc.py`、`gen_build_time.cmake` |
| `docs/` | 仅 `SYSTEM_DESIGN.md`（TUN 门禁目标）+ `RELEASE_CANDIDATES.md`（晋升台账） |

`import_anki.py`、`gen_zh_subset.py`、`pack_static_assets.py`、`pixelpost.py`、`benchmark.c`、`balance_sim.c` 都在 playbook `tools/`。

## 本仓禁止的内容

研究文档、迭代日志、指标体系、scorecard / autotune / diag_*、素材制作管线、
**任何中间素材**（raw / review / trial / motion-v2）、OC 私有源图。
OC 固件目录 `main/sprites/oc/` 与 `sdkconfig.oc` 均 gitignore；OC 构建 overlay 在 playbook。

## 跨仓约定（改这些文件前必读）

- `REPO_PLAYBOOK` 默认 `../moyu-playbook`。`validate-host.sh` 的 balance 门和
  `check_design_doc.py` 从 playbook `tools/` 读 `balance_sim.c` / `benchmark.c`。
- `tests/test_asset_packing.py` 经 `DESKPET_ROOT`（badge 根）读 playbook 的 `hero_flavors.py`。
- playbook `scorecard.py` 用 `BADGE_DIR` 指回本仓任一检出。
- 从预览重打包精灵：在 playbook 跑
  `DESKPET_ROOT=<本仓> python3 tools/pack_static_assets.py`。
- 固件构建：`PATH="$HOME/.espressif/python_env/idf5.5_py3.14_env/bin:$PATH"` 先于
  `source ~/esp/esp-idf-v5.5.3/export.sh`。

## 分支、刷机、工作跟踪

- `main` = 发版线。自治迭代在 `../moyu-badge-autogoal`（worktree，分支 `autogoal`），
  经 `autogoal-rc` 晋升；**本仓只 `git merge autogoal-rc`**。
- 刷机由本仓操作者执行；循环无串口权限。`cardid` / `recovery` 永不写入，禁 `erase-flash`。
- 产品工作图在 Involute **INV-65**（DeskPet）。本仓不另建 TODO 清单。

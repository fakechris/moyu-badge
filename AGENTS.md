# AGENTS.md —— moyu-badge 仓库契约（发版仓）

本仓 = **发版仓**：只放发版本必须的东西。
判定口诀：**「发版本需要它吗？」不是 → 不要放进本仓。**

本文件是进仓铁律。循环操作手册在 gitignored 的 `memory/AGENTS.md`，不得与本节冲突。

## 本仓允许的内容

| 路径 | 内容 |
|---|---|
| `main/` | 固件代码 + `main/sprites/generic` 打包公版素材 + 字体/词表生成产物 |
| `tests/` + `sim/` | 固件回归门（host 可跑）；sim 自包含 vendored LVGL |
| `sender/` | agent 状态 BLE 推送客户端 |
| `tools/` | 仅发布工具：`validate-host.sh`、`flash.sh`、`product_check.sh`、`budget.py`、`check_design_doc.py`、`gen_build_time.cmake` |
| `docs/` | `SYSTEM_DESIGN.md`、`RELEASE_CANDIDATES.md`、`docs/screenshots/`（README 实机图） |

## 本仓禁止的内容

研究文档、迭代日志、指标体系、autotune、素材制作管线、
**任何中间素材**、未公开发布的私有立绘。
`main/sprites/oc/` 与 `sdkconfig.oc` 均 gitignore，不得入库。

## 构建

固件构建：`PATH="$HOME/.espressif/python_env/idf5.5_py3.14_env/bin:$PATH"` 先于
`source ~/esp/esp-idf-v5.5.3/export.sh`。BSP 默认 `../my-ai-passport/ai-passport/components`。

## 分支梯队（必须遵守）

循环在 `../moyu-badge-autogoal`，**每轮都提交**——但提交目标是草稿分支，不是 `main`。

| 分支 | 定位 | 谁写 | 谁读 |
|---|---|---|---|
| `autogoal` | 草稿区。每轮提交；半成品、中间结果、下一轮可回滚 | **只循环**（该 worktree） | 禁止 merge 进 `main` |
| `autogoal-rc` | 候选发布线。只有按条款晋升的提交 | 循环在满足 §5.3 时快进/推进 | **`main` 唯一允许的合入源** |
| `main` | 发版线 | 操作者：`git merge autogoal-rc`；以及本仓发版必需的热修/美术成品 | 刷机、发布 |

**禁止：**

- `git merge autogoal`（合草稿）。永远只 `git merge autogoal-rc`。
- 在主检出上直接提交循环式玩法/数值实验。那类工作只许在 autogoal worktree 的 `autogoal` 分支上进行。
- 循环写入 `main`、切换到 `main`、或碰串口 / `erase-flash` / `cardid` / `recovery`。

晋升条款是书面合同，不是循环记忆。全部满足才准动 `autogoal-rc`：

1. 该提交自身全套件绿；触及 `main/` 固件的轮次还要**同轮** `idf.py build` 绿。
2. **浸润**：固件杠杆必须在其后一轮复测仍达标、门仍绿（至少测两轮）。纯 tools/docs 可当轮晋升。
3. 杠杆完整，非半成品，无已知未决回归。
4. 写入 `docs/RELEASE_CANDIDATES.md`（commit、杠杆、指标增量、约 5 分钟真机核对清单、回滚指针），并打 tag `autogoal-rc-N`。

循环验证不了真机手感。它只把过了客观门槛 + 浸润的提交递到 rc，并附刷机前清单。刷机由本仓操作者做。

产品工作图在 Involute **INV-65**。本仓不另建 TODO 清单。

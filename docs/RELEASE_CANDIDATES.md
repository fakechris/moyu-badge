# RELEASE_CANDIDATES —— autogoal → main 的候选发布台账

> merge 规则（与根 `AGENTS.md` 相同）：主检出永远只 `git merge autogoal-rc`。
> 不要 merge `autogoal`——那是工作草稿，每轮都在变，含未浸润的中间结果。
> 晋升标准：`../moyu-playbook/docs/OPTIMIZATION_GOAL.md` **§5.3**（书面条款，不是循环记忆）。
> 循环无法验证真机手感，每次晋升附真机核对清单，用户过一遍（~5 分钟）再刷机。

## autogoal-rc-1 (当前分支尖)

- **commit**: 含至「autogoal: device-safety invariant」的全部 autogoal 提交
- **内容**: 自治轮1-3（跑步机门修复带意图注记 / 烟遁审计补受击判据 / identity 尺子对齐
  B1 charter）+ 设备安全不变量文档。scorecard 9.0→9.4（risk 7.7→10、identity 7.8→10）
- **固件改动**: 无（三轮全部是 tools/ 与 docs/ 层——balance_sim.c、diag_smoke.c、
  scorecard.py、docs；main/ 固件代码零改动，设备行为与现网固件完全一致）
- **浸润**: tools 层改动，host 套件多轮全绿
- **真机核对清单**: 无需刷机——本候选不影响固件；merge 后设备固件无变化
- **回滚**: `git revert` 对应提交，或 merge 前直接不合

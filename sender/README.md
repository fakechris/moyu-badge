# DeskPet agent sender (laptop side)

把本机 coding agent 状态经 BLE 推给 DeskPet 的**瘦客户端**。状态采集与
归并已委托给 [planofplan](../../planofplan)(`GET /api/agent-status`,
机制与契约文档见 `planofplan/docs/agent-status-api.md`)——本脚本只做
两件事:轮询那个 API,把 4 槽快照打包成 GATT 写入推给 `DeskPet Click`。

## Quick start

```bash
pip install -r requirements.txt        # 只有 bleak
# 前置:planofplan serve 已在跑(默认 127.0.0.1:9288)
python3 deskpet_sender.py --dry-run        # 不碰蓝牙:打印槽位表
python3 deskpet_sender.py --discover       # 验证 DeskPet 的 GATT 服务
python3 deskpet_sender.py                  # 常驻,每 5s 推一次
```

- `--planofplan-url` 换 planofplan 地址;`--disable amp,agy` 临时禁源
  (planofplan 端点的 `disable` 参数也可用,二选一);
- planofplan 短暂失联 ≤30s 时沿用上次快照,更久则清空设备槽位;
- 状态到设备的端到端延迟 ≈ 5-10s(轮询模型,见 API 文档"实时性")。

## 状态源与映射

全部机制、精度分级和 6 态映射表现在由 planofplan 维护
(`docs/agent-status-api.md`);新接 agent 源也改那边,本脚本零改动。
`mapping.md` 保留为当时的源始语义参考。

## Hooks (claude/droid 精确状态)

```bash
bash hooks/install-claude.sh   # appends Stop/UserPromptSubmit/Notification writers
```

Writers 写 `~/.config/deskpet-sender/*.json`;**读取方现在是 planofplan**
的 poller(原来在本目录 sources/,已委托)。

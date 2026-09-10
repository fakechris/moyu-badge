# DeskPet Game — standalone firmware for FoloToy AI Passport

放置迷宫 RPG：OC 自动爬塔（100 层/n 周目），玩家做少数重决策
（切职/技能/三选一/回城）。EN/中文。三键操作（UP/DOWN/OK）。

本仓库 = **游戏本体**：固件代码、构建工具、模拟器、测试、成品精灵。
开发文档/数值方法论/复盘/sprite skill 在姊妹仓库 **`moyu-playbook`**
（入口：`docs/DESKPET_FACTORY.md`）。

## 构建

```bash
source ~/esp/esp-idf-v5.5.3/export.sh
# BSP 默认指向 ../my-ai-passport/ai-passport/components（DESKPET_BSP_DIR 可覆盖）
idf.py build
idf.py -p /dev/cu.usbmodem101 flash monitor   # 只写 factory 分区，禁 erase-flash
```

## 验收（改任何代码/数值后）

```bash
python3 tools/budget.py        # bin 体积 / 精灵预算 / 宿主测试 / 平衡门
cc -O1 -Imain tools/benchmark.c main/dungeon_model.c -o /tmp/bench && /tmp/bench
                               # 玩法验收 benchmark：分布+达标全报告
```

## 布局

```text
main/            app_main + pet/dungeon/pomo/i18n/audio_task + sprites/fonts
tools/           pixelpost(精灵管线) / balance_sim(平衡门) / benchmark(验收报告)
sim/             宿主模拟器（SDL，键盘=三键）
tests/           宿主模型测试（无硬件）
sender/          agent 状态 BLE 推送（瘦客户端，消费 planofplan 的 /api/agent-status）
sprites 内嵌于固件；精灵再生成所需的源素材在 moyu-playbook 仓库 assets/
```

## 双人物素材包（hero art flavors）

人物素材分两套可互换的 flavor；怪物/迷宫/UI/背景为共享素材，两个包完全一致：

| flavor | 人物形象 | 目录 | 用途 |
|---|---|---|---|
| `generic`（默认） | 通用 Q 版冒险者（5 职业） | `main/sprites/generic/`（入库） | 公开发布 |
| `oc` | 私有 OC 人物 | `main/sprites/oc/`（**gitignore，不入库**） | 家庭内供 |

两套文件名完全相同，`main/sprite.c` 与游戏代码不感知 flavor；构建时由
Kconfig `DESKPET_ART_FLAVOR`（menuconfig → DeskPet art）选择嵌入哪一套。

```bash
# 公开发布包（默认 generic）
idf.py -B build-generic -D SDKCONFIG=build-generic/sdkconfig \
  -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults" build

# 家庭内供包（需要本地存在 main/sprites/oc/）
idf.py -B build-oc -D SDKCONFIG=build-oc/sdkconfig \
  -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.oc" build
```

素材再生成：

```bash
python3 tools/build_generic_heroes.py        # 像素规则确定性绘制 generic 全套（无 AI）
python3 tools/pack_static_assets.py          # 预览 -> 固件二进制（所有 flavor）
python3 tools/split_hero_flavors.py          # 一次性迁移工具（从旧版单目录结构恢复）
```

约定：

- `assets/previews/{shared,generic}/` 是入库的 4x 预览源；
  `assets/previews/oc/` 同样 gitignore，永不发布。
- OC 人物设计版权属于家人，任何 OC 像素（`main/sprites/oc/`、
  `assets/previews/oc/`）不得 commit、不得进入公开固件或截图。
- 公开仓库若误选 `oc` flavor 且目录为空，CMake 直接报错，不会静默出包。

## 安全

- `cardid@0x356000` + `recovery@0x700000` 永不写入（上游
  `docs/development/engineering/ble-recovery-compatibility.md`）
- 整机 ROM 备份：`../my-ai-passport/rom-backup/`

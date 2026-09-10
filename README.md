# DeskPet Game — standalone firmware for FoloToy AI Passport

放置迷宫 RPG：OC 自动爬塔（100 层/n 周目），玩家做少数重决策
（切职/技能/三选一/回城）。EN/中文。三键操作（UP/DOWN/OK）。

本仓库 = **发版仓**：固件代码、模拟器、测试、打包公版精灵、发布门。
开发文档 / 数值方法论 / 复盘 / 素材源 / 制作管线在姊妹仓库 **`moyu-playbook`**
（入口：`../moyu-playbook/docs/DESKPET_FACTORY.md`，契约见 `AGENTS.md`）。

## 构建

```bash
PATH="$HOME/.espressif/python_env/idf5.5_py3.14_env/bin:$PATH"
source ~/esp/esp-idf-v5.5.3/export.sh
# BSP 默认指向 ../my-ai-passport/ai-passport/components（DESKPET_BSP_DIR 可覆盖）
idf.py build
tools/flash.sh                     # 只写 factory 分区，禁 erase-flash
```

## 验收（改任何代码/数值后）

```bash
tools/validate-host.sh             # 宿主门（含 playbook 的 balance_sim）
python3 tools/budget.py            # bin 体积 / 精灵预算 / 宿主测试
BADGE_DIR="$PWD" python3 ../moyu-playbook/tools/scorecard.py
cc -O1 -Imain ../moyu-playbook/tools/benchmark.c main/dungeon_model.c \
    -o /tmp/bench && /tmp/bench    # 玩法验收分布
```

## 布局

```text
main/            app_main + pet/dungeon/pomo/i18n/audio_task + sprites/fonts
tools/           发布门：validate-host / flash / product_check / budget / check_design_doc
sim/             宿主模拟器（SDL，键盘=三键）
tests/           宿主模型测试（无硬件）
sender/          agent 状态 BLE 推送
docs/            SYSTEM_DESIGN.md（TUN 门）+ RELEASE_CANDIDATES.md
```

研究工具（benchmark / scorecard / pixelpost / pack_static_assets）和素材源在
**moyu-playbook**。

## 双人物素材包（hero art flavors）

| flavor | 人物形象 | 目录 | 用途 |
|---|---|---|---|
| `generic`（默认） | 公开版烬羽（5 职业） | `main/sprites/generic/`（入库） | 公开发布 |
| `oc` | 私有 OC 人物 | `main/sprites/oc/`（**gitignore**） | 家庭内供 |

两套文件名完全相同，`main/sprite.c` 不感知 flavor；Kconfig `DESKPET_ART_FLAVOR` 选择嵌入哪一套。

```bash
# 公开发布包（默认 generic）
idf.py -B build-generic -D SDKCONFIG=build-generic/sdkconfig \
  -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults" build

# 家庭内供包（需要本地 main/sprites/oc/ + 从 playbook 拷来的 sdkconfig.oc）
cp ../moyu-playbook/sdkconfig.oc .
idf.py -B build-oc -D SDKCONFIG=build-oc/sdkconfig \
  -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.oc" build
```

素材再生成（在 **playbook** 跑，写出 badge）：

```bash
DESKPET_ROOT=/Users/chris/workspace/moyu-badge \
  python3 ../moyu-playbook/tools/pack_static_assets.py
```

约定：

- 公版成品只进 `main/sprites/generic/`。源图、审查图、试做在 playbook `assets/`。
- OC 像素不得 commit、不得进入公开固件或截图。
- 公开构建若误选 `oc` flavor 且目录为空，CMake 直接报错，不会静默出包。

## 安全

- `cardid@0x356000` + `recovery@0x700000` 永不写入
- 整机 ROM 备份：`../my-ai-passport/rom-backup/`

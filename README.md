# 四合一：放置 / 背单词 / 番茄钟 / 翻页笔
## 4-in-1: Idle, Vocab, Pomodoro & Clicker for AI Passport

> **4-in-1 Idle, Vocab, Pomodoro & Clicker for AI Passport · 放置 / 背单词 / 番茄钟 / 翻页笔**  
> 把 [AI Passport](https://ai-passport.folotoy.cn/) 变成装进口袋的多功能桌面伴侣：自带 100 层像素放置迷宫、科学记忆 FSRS 背单词卡、治愈系专注番茄钟、低功耗蓝牙演示翻页笔。三键盲操，中英双语，全离线可用。  
> 
> Transform your AI Passport into a versatile pocket desktop companion: a 100-floor auto-battling idle dungeon RPG, a scientific FSRS vocabulary flashcard system, a focus pomodoro timer with animated pet reactions, and a low-latency Bluetooth LE presentation clicker. Three-key intuitive control, bilingual (EN/ZH), and 100% offline.
> 
> 🚀 **玩法社区已上线 / Live on Play Community**：[AI Passport 玩法社区 · 项目 277 (DeskPet)](https://ai-passport.folotoy.cn/plays/277/)

---

## 实机展示 / On-Device Showcase

<p align="center">
  <img src="docs/screenshots/game.jpg" width="18%" alt="放置迷宫 / Idle Dungeon" />
  <img src="docs/screenshots/modes.jpg" width="18%" alt="模式切换 / Mode Switcher" />
  <img src="docs/screenshots/vocab.jpg" width="18%" alt="背单词 / Vocab Cards" />
  <img src="docs/screenshots/pomo.jpg" width="18%" alt="番茄钟 / Pomodoro Timer" />
  <img src="docs/screenshots/clicker.jpg" width="18%" alt="翻页笔 / Slide Clicker" />
</p>

| 模式 / Mode | 中文说明 | English Description |
|---|---|---|
| 🗡️ **放置迷宫 (Idle Dungeon)** | 100 层爬塔 RPG。九成时间全自动探索打怪，亮屏时切职爆发、选房间卡牌、搜集装备圣所、周目转生。 | 100-floor dungeon RPG. 90% auto-pilot battling. Swap jobs, cast skills, pick room cards, gather loot, sanctum upgrades, and prestige into NG+. |
| 📖 **科学背单词 (Vocab Cards)** | 基于 FSRS 间隔重复算法，内置精选高频核心词库。复习优先，支持三档自评、超额加背、反向回想与三选一强化。 | Powered by the modern FSRS spaced-repetition algorithm. Due reviews prioritized, 3-tier rating, daily quota adjustment, reverse recall, and 3-choice distractor drills. |
| ⏱️ **专注番茄钟 (Pomodoro)** | 经典 25 分钟专注 + 5 分钟小憩循环。像素桌宠跟随工作/休息状态变换姿态，配备复古 8-bit 芯片音提示。 | 25-min focus + 5-min break loop. The pixel companion dynamically changes poses between focus and reward states, with retro chiptune alerts. |
| 📱 **演示翻页笔 (BLE Clicker)** | 免驱低功耗蓝牙 HID 键盘，秒连电脑/平板/手机。PPT/Keynote/PDF 上下翻页、F5 全屏放映、双击黑屏，自动暗屏省电。 | Driver-free Bluetooth LE HID keyboard for PC/Mac/iPad. Page Up/Down, F5 presentation launch, double-click blank screen, and auto-dimming power saving. |
| 🌙 **待机与设置 (Standby & Settings)** | 待机时钟与桌宠睡姿（支持 BLE/串口推送 AI Agent 状态看板）；独立设置菜单调节全局与各分类音效音量及中英语言。 | Standby displays real-time clock & sleeping pet (or live AI Agent status cards); dedicated settings menu for volume levels, chiptunes, and EN/ZH language. |

---

## 全局交互与模式切换 / Navigation & Mode Switching

- **长按 OK（Hold OK）**：在**任何模式**下长按 OK 键，均会立即呼出全局「模式切换」菜单（Mode Switcher）。
- 在模式切换菜单中：
  - **UP / DOWN**：上下移动光标浏览模式（放置、番茄钟、翻页笔、背单词、待机、设置）。
  - **OK**：确认进入选中的模式。
  - **再次长按 OK**：取消切换并返回原模式。
- **状态持久化与独立驻留**：
  - 进入任何模式后，设备绝不会未经操作自动跳出（背完单词停留在结算菜单，做演讲时翻页笔常驻后台，番茄钟未完成绝不退回桌面）。
  - 模式、词卡进度、游戏存档、系统设置均实时写入 NVS 掉电非易失存储，重启开机自动恢复最后使用的模式。

---

## 场景详细玩法 / Detailed Mode Guides

### 1. 🗡️ 放置迷宫 / Mode 1: Idle Dungeon RPG

口袋里的放置小天地。上班摸鱼、工作学习时放在桌边，宠物自己爬 100 层迷宫，无需时刻盯着；当你偶有一分钟闲暇抬眼看它，又能随时上手享受策略微操与成长快感。

#### 玩法机制 / Gameplay Mechanics
- **自动巡航 (90% Idle)**：桌宠自动探索楼层、自动寻路、按职业性格自动索敌攻击、获取金币与经验、拾取掉落物。
- **五大鲜明职业 (5 Distinct Jobs)**：
  - 🛡️ **骑士 (Knight)**：重装防御壁垒，极高守备与生命，盾击打断敌方蓄力，嘲讽聚怪，拥有终极技能「城堡禁区」。
  - 🔥 **黑魔 (Black Mage)**：重火力远程炮台，主攻极致输出，火球/冰锥大范围杀伤，终极技能「陨石降临」。
  - 🌿 **白魔 (White Mage)**：生存保障专家，生命值低于 60% 触发自愈，圣盾吸收伤害，终极技能「圣所领域」持续恢复。
  - 🗡️ **盗贼 (Thief)**：高敏锐高速刺客，首回合必定触发 1.5 倍背刺暴击，涂毒削弱强敌，终极技能「万宝开箱」狂揽战利品。
  - 🩸 **暗骑 (Dark Knight)**：残血收割者，血量越低爆发越强，自带吸血与末日诅咒，终极技能「日蚀狂怒」。
- **双职自由搭配 (Main & Sub Job)**：解锁副职业后，可任意组合主副职业（如骑士主坦 + 白魔副奶，或黑魔主攻 + 盗贼副暴击），战术随心搭配。
- **房间三选一 (Room Card Choices)**：每清完一个区域，会出现宝箱房、许愿池、神秘事件、精英怪挑战等卡牌，自由抉择前进方向。
- **经济闭环与永久成长 (Economy & Progression)**：
  - **迷宫回程机制**：探索中打怪掉落的金币属于「本局携带现金」。在濒危前使用「回城羽毛」或烟遁回城，可将金币存入金库；若战死迷宫，本局现金清空。
  - **永久局外培养**：职业等级、装备锻造等级（武器攻击/防具守备）、圣所力量加护（FOR/VIT/DEF 乘区倍率）、转生徽章（Prestige）在死亡后**全部永久保留**。
  - **100 层周目轮回 (NG+)**：通关第 100 层即开启全新周目，怪物属性与技能更具挑战性，通关奖励与转生加成更为丰厚。

#### 操作按键 / Controls
| 状态 / Context | 按键 / Key | 操作说明 / Action |
|---|---|---|
| **迷宫战斗** | `UP / DOWN` | 上下选择技能（技能 1 / 技能 2 / 技能 3 / 回城） |
| | `OK (单击)` | 手动释放所选技能（按任意键立即介入手动操控接管战斗） |
| | `UP / DOWN (长按)` | 极速切换主副职业带队出战 |
| | `OK (双击)` | 消耗羽毛（Feather）紧急保金币回城 |
| **城镇营地** | `UP / DOWN` | 浏览 9 处设施（巡航开关、铁匠铺强化、合成、圣所加护、旅馆、许愿池、转生、副职切换、出征） |
| | `OK (单击)` | 确认升级/休息/出征 |
| **路线抉择** | `UP / DOWN` | 在 3 个房间卡牌之间切换光标 |
| | `OK (单击)` | 确认进入选定房间 |
| **全局** | `OK (长按)` | 呼出全局模式切换菜单 |

---

### 2. 📖 科学背单词 / Mode 2: Vocab Flashcards (FSRS)

基于现代认知科学与 [FSRS (Free Spaced Repetition Scheduler)](https://github.com/open-spaced-repetition/fsrs4anki) 间隔重复算法设计的掌上单词卡片机。无需掏出手机分心看微信，利用碎片时间在桌边高效完成每日复习与新词积累。

#### 核心亮点 / Core Features
- **科学记忆曲线 (FSRS Engine)**：根据每一次评分精确演算记忆稳定性（Stability）与遗忘难度（Difficulty），杜绝传统机械循环，大幅节省记忆时间。
- **精选中高频词库 + 完整 IPA 音标**：内置高频精选核心词汇，字库针对汉字及国际音标（IPA）深度优化，排版工整清晰。
- **三阶段智能调度 (3-Phase Queue)**：
  1. **到期复习 (Due Reviews)**：优先完成今天已到期的遗忘点卡片；
  2. **每日新词 (New Words)**：在完成复习后自动按配额解锁今日新词；
  3. **当堂重练 (Session Re-drill)**：本次学习中评为「忘记」的生词，会在当次会话末尾不断再现重练，直至掌握为止。
- **三档自评体系 (3-Tier Rating)**：
  - 正面展示英文单词与音标，按 `OK` 翻面显示中文释义；
  - 翻面后三键快速自评：
    - `UP` = **忘记 (Again)**：当堂进入重练环反复强化，并触发 FSRS 失效记录；
    - `OK` = **模糊 (Hard)**：虽能回想但较吃力，缩短下次复习周期；
    - `DOWN` = **认识 (Good)**：熟练掌握，大幅拉长下次复习间隔。
- **学完专享菜单 (Post-Quota Menu)**：完成今日额度后不会粗暴退出，而是进入多维度强化菜单：
  - **超额再背 (Extend Budget)**：随时追加 10 个词继续挑战；
  - **每日新词数 (Daily Cap)**：支持 5 / 10 / 15 / 20 / 30 词灵活步进；
  - **反向回想 (Reverse Recall)**：正面只显示中文释义，在大脑中尝试拼读与发音，按 OK 翻面看英文自评；
  - **反向选择 (Reverse 3-Choice)**：看中文释义，屏幕提供 3 个由算法生成的形近/意近混淆项（Distractors），按 1 (UP) / 2 (OK) / 3 (DOWN) 快速选择，专治"似懂非懂"；
  - **词表顺序 (Order)**：支持顺序播放或随机乱序。
  - **进度预测 (Forecast)**：实时展示当前总已学词汇量及明日预计到期复习卡片数。

#### 操作按键 / Controls
| 状态 / Context | 按键 / Key | 操作说明 / Action |
|---|---|---|
| **正面（看英文）** | `OK (单击)` | 翻面查看中文释义与音标 |
| **背面（自评）** | `UP (单击)` | **忘记 (Again)**：记入生词重练队列 |
| | `OK (单击)` | **模糊 (Hard)**：适中缩短复习间隔 |
| | `DOWN (单击)` | **认识 (Good)**：已掌握，延长复习周期 |
| **反向三选一** | `UP / OK / DOWN` | 分别对应选择 选项 1 / 选项 2 / 选项 3 |
| **功能菜单** | `UP / DOWN` | 浏览菜单项（加背、每日词数、反向回想、反向选择、顺序） |
| | `OK (单击)` | 确认进入该项或循环切换参数 |
| **全局** | `OK (长按)` | 呼出全局模式切换菜单 |

---

### 3. ⏱️ 专注番茄钟 / Mode 3: Pomodoro Focus Timer

桌面上的专注伴侣。将经典番茄工作法（25 分钟沉浸专注 + 5 分钟舒适小憩）与像素宠物的动态交互完美结合，用仪式感告别拖延。

#### 核心亮点 / Core Features
- **极简清晰界面**：大字号剩余倒计时（分:秒）、当前阶段状态标签、累计完成番茄数（xN）、平滑专注进度条。
- **桌宠情感化反馈 (Animated Companion)**：
  - **专注阶段 (Focus)**：桌宠呈现低头伏案工作的静心立绘，与你共同投入任务；
  - **休息与奖励阶段 (Reward/Break)**：桌宠切换为举手欢呼的开心立绘，犒劳你的阶段成果。
- **复古 8-bit 提示音**：专注倒计时结束播放明快的过关音效，休息倒计时结束播放温和提醒音效。
- **长效防打扰与智能节能**：
  - 专注计时运行期间屏幕持续常亮，绝不擅自黑屏或退出模式；
  - 当一个完整专注+休息周期结束、且空闲超过 2 分钟无按键操作时，自动转入低功耗待机，保护电池寿命。

#### 操作按键 / Controls
| 当前状态 / State | 按键 / Key | 操作说明 / Action |
|---|---|---|
| **空闲 (IDLE)** | `OK (单击)` | 开启 25 分钟专注计时 |
| **专注中 (RUNNING)** | `OK (单击)` | 暂停当前计时 |
| **已暂停 (PAUSED)** | `OK (单击)` | 恢复专注计时 |
| **休息/奖励中** | `OK (单击)` | 提前结束休息或结算番茄 |
| **任何进行中状态** | `DOWN (单击)` | 放弃本轮番茄钟（重置为空闲） |
| **全局** | `OK (长按)` | 呼出全局模式切换菜单 |

---

### 4. 📱 演示翻页笔 / Mode 4: BLE Presentation Clicker

将小巧的 AI Passport 变身为你进行技术分享、方案答辩、学术汇报时的无线演讲翻页器。免插接收器、免装驱动，支持各种主流操作系统。

#### 核心亮点 / Core Features
- **标准免驱蓝牙 HID 键盘**：采用低功耗蓝牙（BLE HID）协议，无缝适配 macOS (Keynote / PowerPoint / 浏览器 / PDF 预览)、Windows (PPT / WPS / PDF)、iPadOS、Android 与 Linux。
- **进模式即广播配对**：切换到「翻页笔」模式后，设备自动开启 2 分钟蓝牙广播窗口。在电脑或平板的蓝牙搜索列表中点击即可秒速配对连接。
- **实时连接状态与发包统计**：
  - 屏幕状态清晰标注：未配对时呈黄色 `未配对 / PAIRING`，连接成功即变绿色 `已连接 / LINKED`；
  - 底部实时统计已发送按键包与丢包情况（如 `36/0`），演讲心里有底。
- **演讲防干扰暗屏机制**：
  - 演讲超过 2 分钟无按键动作，屏幕自动将背光降低至极微弱状态，既消除舞台或会议室的反光干扰，又极度节省电量；
  - 只要按下任意翻页键，屏幕瞬间恢复 100% 亮度，翻页指令零延迟发送。

#### 操作按键 / Controls
| 操作场景 / Scenario | 按键 / Key | 发送信号 / Command | 说明 / Details |
|---|---|---|---|
| **向前翻页** | `UP (单击)` | **Page Up / ↑** | 幻灯片退回上一页 |
| **向后翻页** | `DOWN (单击)` | **Page Down / ↓** | 幻灯片前进下一页 |
| **全屏放映** | `OK (单击)` | **F5** | PPT / Keynote 进入全屏放映模式 |
| **黑屏 / 亮屏** | `OK (双击)` | **B (Blank)** | 演讲中切入黑屏，将听众焦点拉回演讲者 |
| **全局** | `OK (长按)` | — | 呼出全局模式切换菜单 |

---

### 5. 🌙 待机与设置 / Mode 5: Standby & Settings

- **待机模式 (Standby)**：
  - 默认展示北京时间精准走时的大字时钟，桌宠进入梦乡打呼；
  - 若配合 `sender/` 工具（支持 BLE 广播与 USB 串口推送），可实时接收并渲染 AI Agent 运行状态看板（工作任务、进度条、需人工确认、测试失败、庆祝完成等）；
  - 无 USB 供电且闲置 30 秒后自动进入 Deep Sleep 超低功耗休眠，轻触任意键即刻以微秒级唤醒。
- **系统设置 (Settings)**：
  - **声音偏好独立开关**：全局静音 (Mute All)、番茄钟提示音、游戏音效、交互音效；
  - **音量分级调节**：BGM 背景音量、SE 音效音量（20% ~ 100% 递增循环）；
  - **系统语言切换**：简体中文 (ZH) 与 English (EN) 全局实时切换；
  - **重新开启蓝牙配对**：手动为翻页笔打开新的蓝牙配对窗口。

---

## 按键操作对照总表 / Keybindings Cheatsheet

| 模式 / Mode | UP (短按) | DOWN (短按) | OK (短按) | OK (双击) | OK (长按) |
|---|---|---|---|---|---|
| 🗡️ **放置战斗** | 上选技能/回城 | 下选技能/回城 | 释放技能/回城 | 羽毛紧急撤退 | **全局切换模式** |
| 🏰 **放置营地** | 上选设施 | 下选设施 | 确认升级/出征 | — | **全局切换模式** |
| 📖 **背词正面** | — | — | 翻面查看释义 | — | **全局切换模式** |
| 📖 **背词背面** | 忘记 (Again) | 认识 (Good) | 模糊 (Hard) | — | **全局切换模式** |
| 📖 **反向三选一** | 选项 1 | 选项 3 | 选项 2 | — | **全局切换模式** |
| ⏱️ **番茄钟** | — | 放弃本轮番茄 | 开始 / 暂停 / 恢复 | — | **全局切换模式** |
| 📱 **演示翻页笔** | 上一页 (PgUp) | 下一页 (PgDn) | 放映演示 (F5) | 黑屏切换 (B) | **全局切换模式** |
| ⚙️ **系统设置** | 光标上移 | 光标下移 | 切换选项 / 调节音量 | — | **全局切换模式** |
| 🌙 **待机时钟** | 切换看板 | 切换看板 | 唤醒设备 | — | **全局切换模式** |

---

## 安装说明 / Installation

### 1. 官方玩法社区一键安装（推荐 / Recommended）
固件已正式上线官方玩法社区，连接设备后直接在浏览器中即可一键免驱动烧录安装：  
👉 **[AI Passport 玩法社区 · 277 号玩法：DeskPet 四合一](https://ai-passport.folotoy.cn/plays/277/)**

The firmware is officially live on the AI Passport Play Community. Connect your device and install with one click directly from your browser!

### 2. 网页端快速刷机 (Web Flasher)
若已获取完整合并固件包，可通过官方网页工具免环境烧录：
1. 用 USB 数据线将 AI Passport 连接至电脑；
2. 打开 [FoloToy 官方网页刷机工具](https://ai-passport.folotoy.cn/tools/web-flasher/)；
3. 选择预编译好的固件 `FoloToy-AI-Passport-full.bin`，烧录起始地址填 `0x0`，点击连接并开始烧录。

### 3. 日常应用分区更新（开发者 / Daily App Flash）
已搭建本地开发环境时，运行仓库内置烧录工具（自动识别串口、只烧录 factory 分区、**保留 NVS 游戏/背词存档**并自动校准设备时钟）：
- **macOS / Linux / Git Bash**：
  ```bash
  tools/flash.sh
  ```
- **Windows (CMD / PowerShell)**：
  ```bat
  python tools/flash.py
  :: 或双击 / 执行：
  tools\flash.bat
  ```
> 🚨 **安全底线**：严禁执行 `erase-flash`！万一遇到固件异常，开机时按住 **UP 键 5 秒** 即可随时进入硬件出厂内置的 Recovery 恢复模式。

---

## 构建与开发 / Build & Development

### 1. 环境准备
- ESP-IDF **v5.5.3**
- Python 3.10+ 环境
- BSP 依赖路径（默认位于 `../my-ai-passport/ai-passport/components`，可通过 `DESKPET_BSP_DIR` 环境变量覆盖）

### 2. 编译与烧录命令
- **macOS / Linux**：
  ```bash
  # 激活 ESP-IDF 环境
  source ~/esp/esp-idf-v5.5.3/export.sh
  # 编译固件
  idf.py build
  # 烧录到设备
  tools/flash.sh
  ```
- **Windows (PowerShell / CMD)**：
  ```powershell
  # 激活 ESP-IDF 环境 (PowerShell)
  . $HOME\esp\esp-idf-v5.5.3\export.ps1
  # 或 CMD: %userprofile%\esp\esp-idf-v5.5.3\export.bat

  # 编译固件
  idf.py build
  # 生成一体化镜像
  idf.py merge-bin -o build/FoloToy-AI-Passport-full.bin
  # 烧录到设备
  python tools/flash.py
  ```

### 3. PC 桌面模拟器 (Simulator)
无需物理设备即可在电脑上调试界面与玩法（支持 macOS / Linux / Windows）：
```bash
# 依赖：SDL2 开发库（Windows 推荐 vcpkg install sdl2 或官网包；macOS: brew install sdl2）
cmake -B sim/build -S sim
cmake --build sim/build --config Release

# 运行模拟器（键盘方向键 = 上下键，Enter/Space = OK 键，长按 >0.9s = 全局模式切换）
# macOS/Linux: ./sim/build/deskpet-sim
# Windows:     sim\build\Release\deskpet-sim.exe
```

### 4. 目录架构说明
```text
main/        固件源码、LVGL 界面、FSRS 核心算法、公版精灵素材、字库产物与词表
tools/       烧录脚本、Host 回归门禁、设计文档校验与固件体积预算工具
sim/         桌面仿真器（可通过 PC 键盘模拟设备三键交互）
tests/       Host 端纯 C 数值与逻辑单元测试集
docs/        系统设计正本 (SYSTEM_DESIGN.md)、发布候选规范 (RELEASE_CANDIDATES.md) 与实机展示图
sender/      AI Agent 状态 BLE / 串口推送客户端
```

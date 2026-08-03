# Fightpad12Slim 菜单与 RGB 子系统实现逻辑

> 适用范围：本仓库 `Fightpad12Slim` 板级配置，代码审计日期为 2026-07-17。  
> 目标：理解拨轮菜单、OLED 接管、RGB 配置、效果渲染、WS2812 输出和 Flash 持久化之间的完整数据流。  
> 注意：这份文档以当前源码为准。`docs/ARCHITECTURE.md` 中旧的 RGB 菜单树和 DIP 控制描述已经落后于当前实现。

## 1. 先建立整体心智模型

这套功能可以分成四层：

```text
GP30/GP31/GP32/GP19
        │
        ▼
ScrollWheelMenuAddon（Core0）
  ├─ 维护菜单层级、光标和按键 FSM
  ├─ 修改 RGB 运行时全局变量
  └─ 发出 GPStorageSaveEvent
        │
        ├──────────────► DisplayAddon（Core1）──► SSD1306 OLED
        │                    读取菜单快照并画文字
        │
        ▼
FightpadAmbientLEDAddon（Core0）
  ├─ 每 20 ms 读取颜色、效果、亮度和电源请求
  ├─ 生成 frame[19] 与 frame_gp22[12]
  ├─ 应用按键闪光、低电关灯和最终黑帧保护
  └─ NeoPico + PIO2 ──► GP40 / GP22 WS2812
```

最重要的三个结论：

1. 拨轮菜单不是 GP2040-CE 原生 `MAIN_MENU` 的一个页面，而是一套独立的 `GPAddon` 状态机。
2. RGB Customize 没有实例化通用 AnimationStation 效果对象；它只复用 `RGB`、`RGB::wheel()` 和全局颜色表，然后在 `FightpadAmbientLEDAddon` 中直接填充 LED 帧。
3. 菜单只负责“选择并发布配置”；真正决定硬件显示结果的是 `FightpadAmbientLEDAddon::render()`、`show()` 和最后的 GP24 电源门控。

## 2. 推荐阅读顺序

| 顺序 | 文件 | 重点 |
|---|---|---|
| 1 | [`configs/Fightpad12Slim/BoardConfig.h`](../configs/Fightpad12Slim/BoardConfig.h) | 引脚、LED 数量、PIO、供电门控宏 |
| 2 | [`headers/addons/scrollwheel_menu.h`](../headers/addons/scrollwheel_menu.h) | 菜单层级、共享状态、运行时 RGB 变量 |
| 3 | [`src/addons/scrollwheel_menu.cpp`](../src/addons/scrollwheel_menu.cpp) | 菜单表、GP30 FSM、导航、应用与保存 |
| 4 | [`src/addons/display.cpp`](../src/addons/display.cpp) | Core1 如何接管 OLED 并显示 `>`/`*` |
| 5 | [`headers/addons/fightpad_ambient_leds.h`](../headers/addons/fightpad_ambient_leds.h) | 双灯链帧缓冲与动画状态 |
| 6 | [`src/addons/fightpad_ambient_leds.cpp`](../src/addons/fightpad_ambient_leds.cpp) | RGB 主循环、各效果、低电保护和 GP24 |
| 7 | [`proto/config.proto`](../proto/config.proto) | RGB 配置的持久化结构 |
| 8 | [`src/config_utils.cpp`](../src/config_utils.cpp) | 持久化字段的默认值 |
| 9 | [`lib/NeoPico/src/NeoPico.cpp`](../lib/NeoPico/src/NeoPico.cpp) | LED 帧如何送进 PIO |

## 3. 硬件与运行核分工

### 3.1 引脚和外设

| 资源 | 当前用途 | 配置依据 |
|---|---|---|
| GP30 | 拨轮按键 SW，短按选择、长按 2 秒进入/退出 | `SCROLLWHEEL_PIN_SW` |
| GP31 | 菜单向下脉冲 | `SCROLLWHEEL_PIN_A` |
| GP32 | 菜单向上脉冲 | `SCROLLWHEEL_PIN_B` |
| GP19 | 返回上一级；板级按键名为 A2/BACK | `SCROLLWHEEL_PIN_BACK` |
| GP22 | 12 颗按键 WS2812，`frame_gp22[12]` | PIO2 SM1，GRB |
| GP40 | 19 颗底部/环境 WS2812，`frame[19]` | PIO2 SM0，GRB |
| GP24 | FP6276 的 RGB 5V Boost 使能 | 高电平启用，All OFF/低电时可拉低 |
| GP00/GP01 | SSD1306 OLED I2C | Core1 的 DisplayAddon 使用 |

当前板级宏位于 [`BoardConfig.h`](../configs/Fightpad12Slim/BoardConfig.h#L126)：

```cpp
#define BOARD_LEDS_PIN 22
#define FIGHTPAD12SLIM_AMBIENT_OUTPUT_PIN 40
#define FIGHTPAD12SLIM_AMBIENT_LEDS_COUNT 19
#define FIGHTPAD12SLIM_AMBIENT_GP22_LEDS_COUNT 12
#define FIGHTPAD12SLIM_AMBIENT_PIO pio2
#define FIGHTPAD12SLIM_AMBIENT_PIO_SM 0
#define FIGHTPAD12SLIM_AMBIENT_GP22_PIO_SM 1
#define FIGHTPAD12SLIM_AMBIENT_POWER_GATE_WHEN_OFF 1
```

### 3.2 双核分工

| Core | 相关对象 | 工作 |
|---|---|---|
| Core0 | `ScrollWheelMenuAddon` | GPIO 读取、按键 FSM、菜单导航、修改 RGB 状态 |
| Core0 | `FightpadAmbientLEDAddon` | 读取手柄状态、生成两条 RGB 帧、驱动 PIO、处理 GP24 |
| Core0 | `GP2040::checkSaveRebootState()` | 收到保存事件后调用 Storage 写 Flash |
| Core1 | `DisplayAddon` | 读取菜单快照、接管 OLED、绘制菜单 |
| Core1 | `FightpadBQ27220BatteryAddon` | 生成低电关灯状态，供 RGB Addon 原子读取 |

注册顺序很重要：Core0 先加载菜单，再加载 RGB Addon：

```cpp
addons.LoadAddon(new ScrollWheelMenuAddon());
addons.LoadAddon(new FightpadAmbientLEDAddon());
```

因此菜单 `setup()` 会先从 Flash 恢复全局 RGB 状态，RGB Addon 随后的 `setup()` 能直接使用恢复后的值。

## 4. 菜单子系统

### 4.1 为什么是独立 Addon

`ScrollWheelMenuAddon` 直接继承 `GPAddon`。它不创建 `GPScreen` 子类，也不进入原生 `DisplayMode::MAIN_MENU` 生命周期。

当 `g_scrollWheelMenuActive` 变为 `true` 后，Core1 的 `DisplayAddon::process()` 提前执行：

```cpp
if (g_scrollWheelMenuActive) {
    drawScrollWheelMenu();
    return;
}
```

也就是说，菜单激活期间，普通 `ButtonLayoutScreen`、原生主菜单等屏幕都会暂时让路；退出后恢复原屏幕流程。

### 4.2 菜单数据模型

每个菜单项使用同一个结构：

```cpp
struct SWMenuItem {
    const char* label;
    SWMenuLevel targetLevel;
    uint8_t targetIndex;
};
```

字段被有意复用：

- `label`：OLED 显示文字。
- `targetLevel`：进入的下一级；对于立即执行的叶子动作，常用 `INFO` 作为“终端”标记。
- `targetIndex`：根据菜单不同，可表示颜色编号、效果编号或亮度档位。

不要把“菜单数组中的行号”和“效果编号”混为一谈。例如 Key Effect 菜单第 2 行是 `Gradient`，但它的效果编号是 `6`。

### 4.3 当前菜单树

```text
MAIN
├─ RP2350B FW Version ─────────────► INFO
├─ ESP32C6 Status ─────────────────► INFO
├─ Battery Info ───────────────────► BATTERY_INFO（代码保留，当前板级宏隐藏入口）
└─ RGB Customize ──────────────────► RGB_SUB
   ├─ Key Flash ───────────────────► COLOR
   │  └─ OFF / Red / Orange / Yellow / Green / Cyan / Blue / Purple / White
   ├─ Key Effect ──────────────────► BUTTON_EFFECT
   │  ├─ Static Color ─────────────► COLOR_BTN，最终效果编号 0
   │  ├─ Gradient ─────────────────► 立即应用效果编号 6
   │  ├─ Breathing ────────────────► COLOR_BTN_BREATH，最终效果编号 4
   │  ├─ Rainbow ──────────────────► 立即应用效果编号 1
   │  └─ Chase ────────────────────► 立即应用效果编号 2
   ├─ Base Effect ─────────────────► AMBIENT_EFFECT
   │  ├─ Static Color ─────────────► COLOR_AMB，最终效果编号 0
   │  ├─ Gradient ─────────────────► 立即应用效果编号 1
   │  ├─ Chase ────────────────────► 立即应用效果编号 2
   │  ├─ Breathing ────────────────► COLOR_AMB_BREATH，最终效果编号 5
   │  └─ Rainbow ──────────────────► 立即应用效果编号 4
   ├─ Brightness ──────────────────► BRIGHTNESS
   │  └─ Bright(0) / Normal(1) / Dim(2)
   └─ All OFF ─────────────────────► 立即执行，不进入下一级
```

`SCROLLWHEEL_BATTERY_INFO_MENU_ENABLED=0` 只移除了 `MAIN` 数组中的入口，没有删除 `BATTERY_INFO` 层级和四页绘制代码。

### 4.4 GP30 的五状态 FSM

GP30 不是简单的“按下计时”。它先经过 30 ms 数字滤波，然后进入五状态 FSM：

```text
                 稳定按下 30 ms
BTN_IDLE ───────────────► BTN_DEBOUNCE_PRESS ─────────► BTN_PRESS
   ▲                            │                          │
   │                            └─ 抖动释放 ───────────────┘
   │                                                       │ 持续 2000 ms
   │                                                       ▼
   └──── 稳定释放 30 ms ◄── BTN_DEBOUNCE_RELEASE ◄──── BTN_LONG
             │
             └─ 如果不是从 BTN_LONG 来的，调用 navSelect()
```

关键行为：

- `BTN_PRESS` 达到 2000 ms：立即调用 `navToggle()`，进入或退出菜单。
- 短按只在稳定释放后调用 `navSelect()`。
- 从 `BTN_LONG` 进入释放状态时，`btnFromLong=true`，因此不会再补发一次短按。
- 任意 GP30 原始边沿都会更新 `g_scrollWheelLastActivityMs`，用于唤醒 OLED 或刷新休眠计时。

这保证了“长按就是长按，短按就是短按”。

### 4.5 GP31/GP32 和 GP19

当前实现没有做完整的 A/B 相位四态解码，而是把两个输入当作简单方向脉冲：

- GP31 出现按下边沿：`navDown()`。
- GP32 出现按下边沿：`navUp()`。
- 共用 80 ms 的 `lastRotaryTime` 防抖。
- `INFO` 详情页禁用旋转；Battery Info 仍可旋转翻页。
- GP19 出现按下边沿：`navBack()`。

因此学习这段代码时，应把它理解成“两个方向 GPIO 按键”，而不是标准机械编码器的格雷码解码器。

### 4.6 导航函数的职责

| 函数 | 职责 |
|---|---|
| `navUp()` / `navDown()` | 循环移动光标，并修正滚动窗口 |
| `navSelect()` | 进入子层、立即应用颜色/效果/亮度、翻电池页或执行 All OFF |
| `navBack()` | 按当前层级回到明确的父层，并恢复父层光标 |
| `navToggle()` | 长按时进入 MAIN，或直接退出整套菜单 |

`navSelect()` 是菜单与 RGB 子系统的核心连接点。它处理四类动作：

1. **导航动作**：进入 `RGB_SUB`、颜色表或效果表。
2. **立即应用动作**：Gradient、Rainbow、Chase、Brightness。
3. **应用并停留**：选择颜色或亮度后保持在当前列表，便于实时比较。
4. **全局动作**：All OFF 同时修改颜色、效果和电源请求。

### 4.7 菜单状态如何跨核显示

Core0 写入：

```cpp
volatile ScrollWheelMenuState g_menuState;
volatile bool g_scrollWheelMenuActive;
```

Core1 每次绘制前逐字段复制到局部 `snap`：

```cpp
ScrollWheelMenuState snap;
snap.active       = g_menuState.active;
snap.level        = g_menuState.level;
snap.index        = g_menuState.index;
snap.scrollOffset = g_menuState.scrollOffset;
snap.infoSource   = g_menuState.infoSource;
```

OLED 是 128×64，字体按 6×8 字符格绘制，因此约为 21 列×8 行。普通列表的标记规则：

- `>`：当前光标，画在第 0 列。
- `*`：当前已经生效的设置，画在第 20 列。
- 光标和生效项可以不是同一行，便于先浏览再确认当前配置。

## 5. RGB 子系统

### 5.1 谁真正拥有 GP22/GP40

当前 Fightpad 配置设置：

```cpp
#define FIGHTPAD12SLIM_AMBIENT_OWNS_GP22 1
```

这会让 Core1 通用 `NeoPicoLEDAddon::available()` 返回 `false`。`AddonManager` 随即删除该对象，不会调用它的 `setup()` 或 `process()`。

所以当前实际所有权是：

| 输出 | 唯一有效写入者 |
|---|---|
| GP22 按键灯 | Core0 `FightpadAmbientLEDAddon` 的 `neopico_gp22` |
| GP40 环境灯 | Core0 `FightpadAmbientLEDAddon` 的 `neopico` |
| GP24 5V_EN | Core0 `FightpadAmbientLEDAddon::setBoostPower()` |

`src/addons/neopicoleds.cpp` 中仍存在通用灯效和历史白色覆盖代码，但在当前宏配置下不会运行。排查“效果代码正确但硬件不对”时，仍应先确认 `available()` 和最后写入者，而不是只看某个效果函数。

### 5.2 运行时配置变量

| 变量 | 含义 | 特殊值 |
|---|---|---|
| `g_menuRgbTop` | GP22 Key Effect 的基础颜色 | `0xFF` 默认白，`0` 黑/关闭该基础色 |
| `g_menuRgbBottom` | GP40 Base Effect 的基础颜色 | 同上 |
| `g_menuRgbButton` | 按键按下时的闪光颜色 | 同上 |
| `g_menuButtonEffect` | GP22 效果编号 | `0xFF` 回退到效果 0 |
| `g_menuAmbientEffect` | GP40 效果编号 | `0xFF` 回退到效果 0 |
| `g_menuBrightnessLevel` | 共享亮度档位 | `0/1/2` = `0.5/0.3/0.1` |
| `g_menuRgbPowerEnabled` | RGB 总电源运行时请求 | `false` 触发最终黑帧和 GP24 关闭 |

颜色编号直接复用 [`animation.h`](../headers/animationstation/animation.h) 中的 `colors`：

```text
0 Black, 1 White, 2 Red, 3 Orange, 4 Yellow, 5 LimeGreen,
6 Green, 7 Seafoam, 8 Aqua, 9 SkyBlue, 10 Blue, 11 Purple,
12 Pink, 13 Magenta, 14 Indigo, 15 Violet
```

菜单只展示其中九项，但保存值仍处在完整的 0..15 颜色编号空间。

必须区分：

- `0xFF`：用户从未设置，采用默认行为。
- `0`：用户明确选择了黑色/OFF。

### 5.3 启动恢复与持久化

持久化结构位于 `FightpadAmbientLEDOptions`：

```proto
message FightpadAmbientLEDOptions {
    optional uint32 topBoardColorIndex = 1;
    optional uint32 bottomBoardColorIndex = 2;
    optional uint32 buttonFlashColorIndex = 3;
    optional uint32 buttonEffectIndex = 4;
    optional uint32 ambientEffectIndex = 5;
    optional uint32 brightnessLevel = 6;
}
```

完整保存流程：

```text
navSelect()
   │ 修改 g_menuRgb* / g_menu*Effect / g_menuBrightnessLevel
   ▼
persistConfig()
   │ 写 Storage 单例中的 FightpadAmbientLEDOptions
   ▼
EventManager.triggerEvent(GPStorageSaveEvent(false))
   │ 只提出请求，不在菜单函数里直接擦写 Flash
   ▼
GP2040::checkSaveRebootState()
   ▼
Storage::save(false) → ConfigUtils::save() → FlashPROM
```

使用事件延后保存，避免在菜单或 LED 渲染热路径中直接擦写 Flash。

`g_menuRgbPowerEnabled` 没有单独的 protobuf 字段。启动时通过以下组合重建 All OFF：

```text
Top=Black && Bottom=Black && Flash=Black
&& ButtonEffect=0xFF && AmbientEffect=0xFF
```

### 5.4 20 ms RGB 主循环

`FightpadAmbientLEDAddon::process()` 的当前正常路径：

```text
每次 Core0 Addon 循环
  ├─ updateButtonFlash(now)          检测新按键边沿，记录 80 ms 截止时间
  ├─ enabled = g_menuRgbPowerEnabled
  ├─ 若距离上帧不足 20 ms，返回
  ├─ render(now)
  │   ├─ clearFrame()
  │   ├─ 低电保护？是则保持全黑并返回
  │   ├─ enabled=false？保持全黑并返回
  │   ├─ renderAmbient(now)          生成 GP40 frame[19]
  │   └─ renderButtons(now)          生成 GP22 frame_gp22[12]
  └─ show()
      ├─ 再次检查 enabled 与低电保护
      ├─ 必要时先发最终黑帧
      ├─ SetFrame() + Show() 两条灯链
      └─ 必要时延时 1 ms 后关闭 GP24
```

`render()` 和 `show()` 都检查低电/关闭状态，是双层保护：即使某个诊断路径直接调用 `show()`，最终输出仍不会绕过黑帧保护。

### 5.5 Key Effect：GP22，12 颗按键灯

| 编号 | 菜单状态 | 实现 | 亮度来源 | Key Flash |
|---|---|---|---|---|
| 0 | Static Color | 全部使用选中颜色 | Bright/Normal/Dim | 支持 |
| 1 | Rainbow | 每颗 LED 使用不同色轮相位，整体移动 | Bright/Normal/Dim | 支持 |
| 2 | Chase | 3 颗追逐，`0.60/0.25/0.05`，200 ms 前进 | 效果内部固定 | 支持 |
| 3 | 菜单隐藏 | 旧 Static Theme 兼容分支 | 固定 `0.5` | 支持 |
| 4 | Breathing | 选中颜色正弦呼吸 | `0.02..0.5`，周期 2400 ms | 支持 |
| 5 | 菜单隐藏 | 旧 Breathing Rainbow 兼容分支 | 呼吸函数 | 支持 |
| 6 | Gradient | 所有 LED 同一种颜色，颜色随时间往返变化 | Bright/Normal/Dim | 支持 |

这里的命名容易误解：

- `Gradient` 不是空间上的多色渐变；12 颗灯在同一时刻颜色相同，只是整组颜色随时间变化。
- `Rainbow` 才是每颗灯具有不同色轮相位的空间彩虹。

### 5.6 Base Effect：GP40，19 颗环境灯

| 编号 | 菜单状态 | 实现 | 亮度来源 |
|---|---|---|---|
| 0 | Static Color | 全部使用选中颜色 | Bright/Normal/Dim |
| 1 | Gradient | 所有 LED 同色，颜色随时间往返变化 | Bright/Normal/Dim |
| 2 | Chase | 5 颗追逐，`0.05/0.25/0.80/0.25/0.05`，200 ms 前进 | 效果内部固定 |
| 3 | 菜单隐藏 | 旧 Breathing Rainbow 兼容分支 | 效果内部状态 |
| 4 | Rainbow | 每颗 LED 具有不同色轮相位 | Bright/Normal/Dim |
| 5 | Breathing | 选中颜色正弦呼吸 | `0.02..0.5`，周期 2400 ms |

### 5.7 Key Flash 覆盖

`updateButtonFlash()` 比较当前手柄状态和上一帧状态，只处理新按下边沿：

```cpp
newButtons = currentButtons & ~lastGamepadButtons;
newDpad    = currentDpad & ~lastGamepadDpad;
```

命中的物理按键通过 `LEDS_BUTTON_*` / `LEDS_DPAD_*` 映射到 GP22 LED 索引，并写入：

```text
gp22FlashUntil[led] = now + 80 ms
```

Key 效果渲染每颗灯时都执行：

```text
now < gp22FlashUntil[led] ? flashColor@0.8 : 当前基础效果
```

因此 Key Flash 是效果之上的短时覆盖层，不会修改基础效果、基础颜色或持久化配置。

### 5.8 亮度档位的真实作用范围

| 档位 | 浮点倍率 |
|---|---|
| Bright | `0.5f` |
| Normal | `0.3f` |
| Dim | `0.1f` |

只应用于：

- Key Static Color、Gradient、Rainbow。
- Base Static Color、Gradient、Rainbow。

不应用于：

- Key/Base Chase：使用自己的梯度系数。
- Key/Base Breathing：使用 `0.02..0.5` 呼吸曲线。
- Key Flash：固定 `0.8f`。

所以选择 Dim 后，如果按键闪光或 Chase 看起来仍然很亮，这是当前设计，而不是亮度菜单失效。

### 5.9 All OFF、低电保护与 GP24

#### All OFF

菜单动作同时执行：

```cpp
g_menuRgbTop = 0;
g_menuRgbBottom = 0;
g_menuRgbButton = 0;
g_menuButtonEffect = 0xFF;
g_menuAmbientEffect = 0xFF;
g_menuRgbPowerEnabled = false;
```

#### 低电保护

BQ27220 发布 `SOC <= 7%` 原子状态。RGB Addon 将它视为比菜单设置更高优先级的临时覆盖：

- 不改颜色、效果、亮度或 Flash 配置。
- 输出强制全黑。
- SOC 恢复到 8% 后自动恢复原灯效。

#### GP24 关闭时序

```text
关闭请求
  ├─ 清空两条 frame
  ├─ GP40 Show() 发送黑帧
  ├─ GP22 Show() 发送黑帧
  ├─ 等待 1000 us，保证最后像素与 WS2812 latch 完成
  └─ GP24 拉低，关闭 RGB 5V Boost

重新开启
  ├─ GP24 拉高
  ├─ 等待 5 ms 电源稳定
  └─ 发送恢复后的第一帧
```

选择单个颜色列表中的 `OFF` 只会把对应颜色设为黑色，不一定关闭 GP24。只有 `All OFF` 或低电保护会提出总关闭请求。

## 6. 四个端到端例子

### 6.1 选择 `Key Effect → Static Color → Red`

```text
GP30短按 Static Color
  → level = COLOR_BTN

GP30短按 Red
  → g_menuRgbTop = 2
  → g_menuButtonEffect = 0
  → g_menuRgbPowerEnabled = true
  → persistConfig()

下一次 RGB 20 ms 帧
  → renderButtons()
  → menuIndexToColor(2) = ColorRed
  → 12颗 GP22 灯显示红色
  → 某按键新按下时，该灯临时显示 Key Flash 颜色 80 ms
```

### 6.2 选择 `Base Effect → Chase`

```text
navSelect()
  → g_menuAmbientEffect = 2
  → g_menuRgbBottom = 0xFF
  → g_menuRgbPowerEnabled = true
  → 保存

renderAmbient()
  → 每 200 ms 移动 ambientChasePixel
  → 计算 5 个相邻 LED
  → RGB::wheel() 产生动态色相
  → 乘以 0.05/0.25/0.80/0.25/0.05
  → 写入 frame[19]
```

Chase 主动把静态颜色恢复为 `0xFF`，防止之前保存的 White/Red 等静态颜色继续影响动态色轮。

### 6.3 选择 Brightness

```text
GP30短按 Normal
  → g_menuBrightnessLevel = 1
  → 保存并留在 BRIGHTNESS 列表
  → OLED 在 Normal 行显示 *

下一帧
  → getMenuEffectBrightness() 返回 0.3f
  → 仅指定效果分支使用新倍率
```

### 6.4 选择 All OFF，再选择 Rainbow

```text
All OFF
  → 两条灯链发送黑帧
  → GP24 拉低

选择 Key Rainbow
  → g_menuButtonEffect = 1
  → g_menuRgbPowerEnabled = true
  → 下一次 show() 先拉高 GP24并等待5ms
  → 再发送 Key Rainbow 帧
```

## 7. 动画状态之间的耦合

理解灯效速度时要注意：

- Base Gradient/Rainbow/Chase 和 Key Rainbow/Chase 共用 `wheelFrame`/`wheelReverse`。
- `renderAmbient()` 总是先于 `renderButtons()`。
- 如果两边同时选择会推进 `wheelFrame` 的效果，一次 20 ms 渲染中可能推进两次，视觉速度会互相影响。
- Key Gradient 使用独立的 `buttonGradientFrame`，因此特意避免了这种耦合。
- Key/Base 单色 Breathing 都由 `now % 2400` 计算，因此天然同步。

如果未来要让 Key 和 Base 的动画速度完全互不影响，应给两条链分别维护 wheel/chase/breath 状态。

## 8. 当前代码中的遗留点和学习陷阱

这些内容是当前源码事实，本次文档任务没有修改它们。

### 8.1 旧 DIP 控制代码已不在主运行路径

`FightpadAmbientLEDAddon` 仍保留：

- `readControls()`
- `handleControlEdges()`
- `previousEffect()` / `nextEffect()`
- `g_scrollWheelButtonLongPressed` 仲裁逻辑

但当前 `process()` 没有调用 `readControls()` 或 `handleControlEdges()`，并明确注释 GP30..32 现在属于拨轮菜单。因此：

- 当前灯效由菜单全局变量驱动。
- GP30 短按在菜单外不会切换 RGB 总开关。
- `BoardConfig.h` 中把 GP30..32 描述成 Ambient ON/OFF/PREV/NEXT 的注释属于历史描述。
- `g_scrollWheelButtonBusy` 当前只有写入，没有读取者。

### 8.2 `g_menuStateDirty` 当前未被消费

菜单操作会把 `g_menuStateDirty=true`，但当前没有代码读取或清除它。DisplayAddon 在菜单激活时会持续清屏并重画，而不是只在 dirty 时刷新。

### 8.3 共享菜单快照不是事务型快照

`g_menuState` 使用 `volatile`，Core1 逐字段读取，没有版本号或临界区。单字节字段写入很简单，但在层级切换瞬间，理论上可能读到新旧字段组合。

如果未来菜单状态变大或加入指针/字符串，应改为版本化快照、临界区或消息队列。

### 8.4 Key Breathing 返回光标存在索引偏差

当前 Key Effect 菜单顺序是：

```text
0 Static Color
1 Gradient
2 Breathing
3 Rainbow
4 Chase
```

但从 `COLOR_BTN_BREATH` 执行 BACK 时，代码把父层光标设为 `1`，会回到 `Gradient` 行，而不是 `Breathing` 行。它不改变已经应用的效果，只影响返回后的光标位置。

### 8.5 滚动窗口存在 6 行与 8 行两套数字

- `clampScrollOffset()` 在光标越过 6 行时开始滚动。
- `DisplayAddon` 实际定义 `SW_MAX_ROWS=8` 并最多画 8 行。

这不会导致越界，但会让列表比 OLED 实际容量更早滚动。

### 8.6 隐藏效果分支仍需保留兼容性

菜单没有入口不代表渲染 case 可以直接删除：旧 Flash 配置可能仍保存 Key `3/5` 或 Base `3`。当前 `switch` 保留这些 case，是为了旧配置不会落入不可预测状态。

### 8.7 保存可能受 USB 认证模式限制

菜单使用 `GPStorageSaveEvent(false)`。`Storage::save(false)` 在特定 PS4/PS5 USB 认证占用场景会拒绝写入，以避免破坏 USB Host 认证流程。运行时效果仍立即生效，但该次修改可能没有落入 Flash。

## 9. 如何安全增加一个新效果

以“给 Key Effect 增加新效果”为例：

1. 在 `kMenuButtonEffects[]` 增加菜单项，并分配一个不破坏旧值的新 `targetIndex`。
2. 更新 `FightpadAmbientLEDOptions.buttonEffectIndex` 注释中的合法范围。
3. 在 `renderButtons()` 增加对应 `case`。
4. 明确颜色来源：选中颜色、动态色轮，还是独立色表。
5. 明确亮度来源：共享 Brightness、效果内部梯度，还是呼吸函数。
6. 所有 Key 效果都应明确 Key Flash 是否覆盖基础帧。
7. 动态效果如果需要动画状态，决定是复用状态还是新建独立状态；优先避免与 Base 速度耦合。
8. 决定切换到该效果时是否要清空静态颜色；只有 Chase 这类完全接管颜色的效果才需要。
9. 验证 OLED 的 `*` 是否能按 `targetIndex` 标记正确行。
10. 验证保存、重启恢复、All OFF 后恢复和低电保护。

不要把“菜单行号”直接当成效果编号，否则插入或调整菜单顺序后，旧 Flash 配置会指向错误效果。

## 10. 建议的实机验证清单

### 菜单

- GP30 短按不会触发长按动作。
- GP30 长按进入/退出后不会再补发短按。
- GP31/GP32 能循环移动光标。
- INFO 页旋转无效，GP19 能返回。
- 颜色和亮度选择后停留在原列表。
- `>` 表示光标，`*` 表示已生效值。

### RGB

- Key/Base 两条灯链可以选择不同效果。
- Static Color、Breathing 使用各自选中的颜色。
- Gradient 是全灯同步变色，Rainbow 是逐灯不同相位。
- Key Flash 只覆盖被按下的对应 LED，持续约 80 ms。
- Bright/Normal/Dim 不应改变 Chase、Breathing 和 Flash 的设计亮度。
- All OFF 先熄灭两条灯链，再测量 GP24 应为低电平。
- 从 All OFF 选择可见效果后，GP24 恢复高电平且第一帧正常。
- SOC 从 8% 降到 7% 时关灯，恢复到 8% 后恢复原配置。
- 重启后颜色、效果和亮度恢复一致。

## 11. 一句话定位问题的方法

| 现象 | 优先检查 |
|---|---|
| OLED 光标不动 | `ScrollWheelMenuAddon::process()` 的 GP31/GP32 边沿与 80 ms 防抖 |
| 长按后又执行短按 | `BTN_LONG → BTN_DEBOUNCE_RELEASE` 的 `btnFromLong` |
| 菜单值变了但 `*` 不对 | `targetIndex` 与 `activeVal` 是否同一编号空间 |
| RGB 改了但重启丢失 | `persistConfig()`、保存事件和 `Storage::save(false)` 返回条件 |
| 效果函数正确但灯不对 | `available()`、Addon 所有权、`show()` 前最后一次帧覆盖 |
| 只有 Key Flash 不对 | `LEDS_BUTTON_*` 映射和 `gp22FlashUntil[]` |
| All OFF 灯黑但 GP24 仍高 | `g_menuRgbPowerEnabled → enabled → show() → setBoostPower(false)` |
| 低电恢复后配置丢失 | 不应修改菜单配置；检查是否只做了瞬时 render override |

掌握这条主线后，再修改菜单或 RGB 时，应始终按“输入事件 → 菜单状态 → 持久化配置 → 效果选择 → 帧缓冲 → 最终写入者 → 电源门控”的顺序检查。

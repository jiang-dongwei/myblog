# GP2040-CE 项目架构文档

## 项目概述

**GP2040-CE** 是一款基于 Raspberry Pi RP2040/RP2350 的开源格斗手柄固件。本项目（Fightpad12Slim）基于 GP2040-CE v0.7.12，使用 **SparkFun Pro Micro RP2350** 开发板，针对自研硬件 Fightpad12Slim 进行深度定制。

- **仓库**: [OpenStickCommunity/GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE)
- **开发板**: RP2350B (双核 Cortex-M33, 150MHz)
- **编译平台**: `rp2350-arm-s`, GCC 14.2.1
- **构建系统**: CMake 3.10+ + Ninja

---

## 1. 目录结构

```
GP2040-CE/
├── CMakeLists.txt              # 根构建文件（~373行）
├── compile_proto.cmake         # nanopb protobuf 编译脚本
├── src/                        # 全部 C/C++ 源码
│   ├── main.cpp                # 入口点：双核启动
│   ├── gp2040.cpp              # Core0 主逻辑：GPIO扫描→手柄管线→USB发送
│   ├── gp2040aux.cpp           # Core1 主逻辑：显示/电池/LED 等外设
│   ├── gamepad.cpp             # 手柄状态机：按键映射→SOCD清洗→热键
│   ├── addonmanager.cpp        # GPAddon 插件管理器
│   ├── storagemanager.cpp      # 存储管理器（Flash/EEPROM 模拟）
│   ├── config_utils.cpp        # Protobuf 配置序列化/反序列化
│   ├── eventmanager.cpp        # 事件系统（按钮按下/释放/菜单导航等）
│   ├── drivermanager.cpp       # USB 驱动工厂（XInput/Switch/PS4/PS5...）
│   ├── usbdriver.cpp           # USB 设备驱动 + TinyUSB 回调
│   ├── webconfig.cpp           # WebConfig HTTP 服务器 + REST API
│   ├── system.cpp              # 系统信息（flash 大小等）
│   ├── addons/                 # 30+ 个 GPAddon 插件
│   ├── display/                # GPGFX 图形引擎 + UI 框架 + 屏幕类
│   ├── drivers/                # 17 种手柄协议驱动
│   ├── gamepad/                # GamepadState 数据结构 + SOCD 清洗
│   ├── interfaces/i2c/         # I2C 设备驱动（SSD1306/PCF8575/ADS1219）
│   └── animationstation/       # LED 动画引擎 + 效果库
├── headers/                    # 头文件（镜像 src/ 结构）
├── lib/                        # 15 个第三方/本地库
│   ├── FlashPROM/src/          # Flash 存储（32KB EEPROM 模拟）
│   ├── nanopb/                 # 嵌入式 Protobuf 实现
│   ├── NeoPico/src/            # WS2812 LED 驱动 (PIO)
│   ├── OneBitDisplay/          # OLED/LCD 单色显示驱动
│   ├── tinyusb/                # TinyUSB USB 协议栈
│   ├── pico_pio_usb/           # PIO 实现的 USB 主机/设备
│   ├── httpd/                  # 嵌入式 HTTP 服务器
│   ├── lwip-port/              # lwIP TCP/IP 栈移植
│   ├── rndis/                  # USB RNDIS 网络
│   ├── CRC32/                  # CRC32 校验
│   ├── ADS1219/                # I2C ADC 驱动
│   ├── ADS1256/                # SPI ADC 驱动
│   ├── PicoPeripherals/        # RP2040 外设抽象
│   ├── WiiExtension/           # Wii 扩展控制器
│   └── SNESpad/                # SNES 手柄协议
├── configs/                    # 50+ 板级配置目录
│   └── Fightpad12Slim/         # 本项目板配置
│       ├── BoardConfig.h       # GPIO 引脚/功能宏定义（核心硬件描述）
│       ├── Fightpad12Slim.cmake# CMake 板级参数
│       ├── fightpad12slim.h    # 引脚别名头文件
│       └── README.md
├── proto/                      # Protobuf 定义
│   ├── config.proto            # 主配置消息（~975行）
│   └── enums.proto             # 枚举定义
├── www/                        # WebConfig 前端（Vite + TypeScript + React）
├── modules/                    # CMake 辅助模块
└── .emv2/                      # 项目开发管理（em 工具）
```

---

## 2. 双核架构

### 2.1 启动流程

```
main() [src/main.cpp]
  ├─ GP2040::setup()   → 初始化存储/USB/SPI/I2C/GPIO/Addon
  ├─ 启动 Core1: multicore_launch_core1(core1)
  │   └─ GP2040Aux::setup() → 初始化 Core1 Addon（显示/电池/LED）
  │   └─ GP2040Aux::run()   → 无限循环：PreprocessAddons → ProcessAddons → processAux
  └─ GP2040::run()     → 无限循环（详见 2.3）
```

### 2.2 核心分工

| | Core0 (GP2040) | Core1 (GP2040Aux) |
|---|---|---|
| **职责** | 游戏手柄管线 | 外设驱动/渲染 |
| **频率** | 1ms USB 轮询速率 | 尽力而为（受 I2C 阻塞） |
| **关键数据** | GPIO→GamepadState→HID Report | 读取 processedGamepad 渲染显示 |
| **Addon** | 输入、修改器、环境灯、菜单导航 | 显示、电池、PLED、蜂鸣器、震动 |

### 2.3 Core0 主循环（手柄管线）

```
GP2040::run() 每次迭代：
  [1] 检测 Profile 变更 → 重新初始化 GPIO 映射
  [2] 保存上一帧状态
  [3] debounceGpioGetAll()        ← 逐 pin 去抖，写入 gamepad->debouncedGpio
  [4] gamepad->read()             ← 读取 debouncedGpio → 填充 GamepadState
  [5] checkRawState()             ← 触发原始按钮事件
  [6] USBHostManager::process()
  [7] addons.PreprocessAddons()   ← ScrollWheel/Encoder 等输入 Addon
  [8] gamepad->process()          ← 轴反转 → 4-way 过滤 → SOCD 清洗 → 摇杆转换
  [9] addons.ProcessAddons()      ← ScrollWheel/FightpadAmbientLED/Turbo/Reverse/Macro
  [10] gamepad->hotkey()          ← 热键检测与执行（菜单导航/档位切换/系统操作）
  [11] rebootHotkeys.process()    ← 重启热键（S2+B3+B4 长按4s）
  [12] checkProcessedState()      ← 触发处理后按钮事件
  [13] memcpy processedGamepad    ← 快照到 Core1 可读的共享内存
  [14] inputDriver->process()     ← 协议驱动生成 HID 报告 + USB 发送
  [15] tud_task()                 ← TinyUSB 维护
  [16] addons.PostprocessAddons() ← ESP32 代理帧发送等
```

---

## 3. GPAddon 插件系统

### 3.1 基类接口

```cpp
class GPAddon {
public:
    virtual bool available() = 0;       // 自检：该设备是否应激活
    virtual void setup() = 0;           // 一次性初始化
    virtual void preprocess() = 0;      // 在 gamepad->process() 之前
    virtual void process() = 0;         // 在 gamepad->process() 之后
    virtual void postprocess(bool) = 0; // USB 报告发送之后
    virtual void reinit() = 0;          // Profile 切换回调
    virtual std::string name() = 0;     // 唯一名称标识
};
```

### 3.2 注册机制

所有 30 个 Addon 源文件**无条件编译**（`CMakeLists.txt` 显式列出），运行时通过 `available()` 自选激活：

```
GP2040::setup():
  addons.LoadAddon(new ScrollWheelMenuAddon());    // SCROLLWHEEL_MENU_ENABLED 宏
  addons.LoadAddon(new FightpadAmbientLEDAddon()); // FIGHTPAD12SLIM_AMBIENT_ENABLED 宏
  addons.LoadUSBAddon(new KeyboardHostAddon());    // 还会注册 USB 监听器
  ...
```

`LoadAddon()`: 调用 `addon->available()` → 若 true 则 `setup()` 并存入 vector → 若 false 则立即 delete。

### 3.3 Fightpad12Slim Addon 列表

**Core0 注册** (按顺序):
| 序号 | Addon | 作用 |
|------|-------|------|
| 1 | KeyboardHostAddon | USB 键盘主机 |
| 2 | GamepadUSBHostAddon | USB 手柄主机 |
| 3 | AnalogInput | 模拟摇杆输入 |
| 4 | HETriggerAddon | 霍尔效应扳机 |
| 5 | BootselButtonAddon | BOOTSEL 按钮 |
| 6 | DualDirectionalInput | 双方向输入合并 |
| 7 | FocusModeAddon | 竞技模式 |
| 8-9 | I2C/SPI Analog | ADC 模拟输入 |
| 10-11 | Wii/SNES | 扩展控制器 |
| 12 | SliderSOCDInput | 滑块 SOCD |
| 13 | TiltInput | 倾斜感应 |
| 14 | RotaryEncoderInput | 旋转编码器 |
| 15 | PCF8575Addon | I2C GPIO 扩展 |
| 16 | TG16padInput | TG16 手柄 |
| 17 | **FightpadESP32ProxyAddon** | ESP32-C6 BT 代理 |
| 18 | **ScrollWheelMenuAddon** | 拨轮菜单系统 |
| 19 | **FightpadAmbientLEDAddon** | 环境灯控制 |
| 20 | ReverseInput | 输入反转 |
| 21 | TurboInput | 连发 |
| 22 | InputMacro | 宏 |

**Core1 注册**:
| 序号 | Addon | 作用 |
|------|-------|------|
| 1 | **DisplayAddon** | OLED 显示 |
| 2 | FightpadBQ27220BatteryAddon | 电池电量 |
| 3 | NeoPicoLEDAddon | WS2812 按键灯 |
| 4 | PlayerLEDAddon | 玩家指示灯 |
| 5 | BoardLedAddon | 板载 LED |
| 6 | BuzzerSpeakerAddon | 蜂鸣器 |
| 7 | DRV8833RumbleAddon | 震动马达 |
| 8 | ReactiveLEDAddon | 反应式 LED |

---

## 4. 手柄输入管线

### 4.1 GPIO 扫描 → 去抖

```
GPIO 引脚 ─→ 扫描（gpio_get_all）──→ 逐 pin 去抖 ──→ gamepad->debouncedGpio (32-bit mask)
```

每个 GPIO 通过 `BoardConfig.h` 中的 `GPIO_PIN_XX` 宏映射到 `GpioAction` 枚举。`ASSIGNED_TO_ADDON` 的引脚不参与手柄扫描。

### 4.2 GamepadState 结构

```cpp
struct GamepadState {
    uint8_t  dpad;          // bits 0-3: dpad方向, bits 4-7: 独立数字方向
    uint32_t buttons;       // 32-bit 按键位掩码 (B1..E12)
    uint16_t aux;           // bit 15: Fn 功能键
    uint16_t lx, ly;        // 左摇杆 0..0xFFFF（中位 0x7FFF）
    uint16_t rx, ry;        // 右摇杆
    uint8_t  lt, rt;        // 模拟扳机 0..0xFF
};
```

### 4.3 SOCD 清洗

支持 5 种模式（Capcom 锦标赛规范）:

| 模式 | 上下同时 | 左右同时 |
|------|---------|---------|
| UP_PRIORITY | 保留上 | 清空 |
| NEUTRAL | 清空 | 清空 |
| SECOND_INPUT | 后按保留 | 后按保留 |
| FIRST_INPUT | 先按保留 | 先按保留 |
| BYPASS | 不处理 | 不处理 |

处理顺序: 轴反转 → 4-way 过滤 → SOCD 清洗 → 摇杆转换

### 4.4 热键系统

最多 16 组热键，每组包含 `buttonsMask + dpadMask + auxMask → action`。

类别：档位切换、SOCD 模式、按键注入、Profile 切换、菜单导航、连发、系统重启等。

---

## 5. 输入模式 / USB 驱动

| InputMode | 驱动类 | 协议 |
|-----------|--------|------|
| XINPUT | XInputDriver | Xbox 360/One (20-byte report) |
| SWITCH | SwitchDriver | Nintendo Switch HID |
| PS3 | PS3Driver | PS3 HID + 认证 |
| PS4 | PS4Driver | DualShock 4 HID + 触摸板/陀螺仪 |
| PS5 | PS5(P5General) | DualSense |
| GENERIC | HIDDriver | 标准 USB HID 手柄 |
| KEYBOARD | KeyboardDriver | USB HID 键盘 6KRO |
| XBONE | XBOneDriver | Xbox One XGIP |

启动模式: 按特定按键上电可切换输入模式（B1=XInput, B3=PS5, B4=PS4 等）。S2 上电进 WebConfig。

---

## 6. 存储系统

### 6.1 FlashPROM (EEPROM 模拟)

```
Flash 布局（32KB @ 0x101F8000）:
┌──────────────────────────────────────┐
│ 0x0000                               │
│         零填充（前导空白）              │
│                                       │
│         Protobuf 序列化数据（可变长）    │
│                                       │
│ 0x7FF4: ConfigFooter (12 bytes)       │
│         ├─ dataSize (4 bytes)         │
│         ├─ crc32    (4 bytes)         │
│         └─ magic    (4 bytes) = 0xd2f1e365 │
└──────────────────────────────────────┘
```

- `start()`: 整块读入 RAM 缓存 `writeCache[0x8000]`
- `commit()`: 50ms 防抖延迟后 → 停双核 → spinlock → 全扇区擦除 + 写回
- 无磨损均衡，每次写入是全扇区擦写

### 6.2 Storage 单例

```cpp
Storage::getInstance()        // Meyer's 单例（C++11 线程安全）
  ├─ getConfig()              // 整个 Config protobuf 对象
  ├─ getGamepadOptions()      // 手柄选项（输入模式/SOCD等）
  ├─ getAddonOptions()        // 所有 Addon 配置
  ├─ getLedOptions()          // LED 配置
  ├─ getDisplayOptions()      // 显示配置
  ├─ getProfileOptions()      // Profile 配置
  ├─ save()                   // 序列化 + 写入 Flash
  └─ setProfile(n)            // 切换按键映射档位
```

### 6.3 Protobuf 配置

`proto/config.proto` 定义了完整的配置结构（proto2 语法，使用 nanopb 固定大小标注）。

顶层 `Config` 消息包含 15 个子消息，嵌套深度可达 3-4 层。所有字段都是 optional，未设置的字段在加载时由 `initUnsetPropertiesWithDefaults()` 填入板级默认值。

### 6.4 WebConfig

内嵌 lwIP HTTPD 服务器 + React SPA。提供完整的 JSON REST API（GET/POST）读写所有配置项。支持在线备份/恢复。

页面功能、接口分组、保存流程、量产默认值和二次开发入口详见
[`WEB_CONFIG_FUNCTIONAL_AND_DEVELOPMENT_GUIDE.md`](WEB_CONFIG_FUNCTIONAL_AND_DEVELOPMENT_GUIDE.md)。

---

## 7. 显示系统

### 7.1 三层架构

```
┌────────────────────────────────────┐
│  Screen 层 (ButtonLayoutScreen 等) │  ← GPScreen 子类
│  Widget 层 (GPButton/GPLever 等)   │  ← GPWidget 子类，屏幕在 Core1 渲染
├────────────────────────────────────┤
│  GPGFX (前端绘图 API)              │  ← drawText/drawRect/drawEllipse...
├────────────────────────────────────┤
│  GPGFX_TinySSD1306 (后端 I2C 驱动) │  ← 1024字节帧缓冲 → SSD1306
└────────────────────────────────────┘
```

### 7.2 DisplayMode 与屏幕生命周期

```cpp
enum DisplayMode {
    CONFIG_INSTRUCTION,  // Web配置模式
    BUTTONS,             // 按键查看器（默认）
    SPLASH,              // 启动画面
    PIN_VIEWER,          // GPIO 状态查看
    DISPLAY_SAVER,       // 屏幕保护
    STATS,               // 系统统计
    MAIN_MENU,           // 迷你菜单
    RESTART,             // 重启通知
    SYSTEM_ERROR         // 错误显示
};
```

启动画面持续 `SPLASH_DURATION` (3s) 后自动切换到 `BUTTONS`。

### 7.3 拨轮菜单接管

当 `g_scrollWheelMenuActive == true` 时，`DisplayAddon::process()` 完全绕过正常的 GPScreen 系统，直接调用 `drawScrollWheelMenu()`：

```cpp
void DisplayAddon::process() {
    if (g_scrollWheelMenuActive) {
        drawScrollWheelMenu();  // 直接渲染菜单，无 GPScreen 对象
        return;
    }
    // ... 正常的屏幕 FSM ...
}
```

这是独立于现有的 `MAIN_MENU` 系统的第二套菜单体系。

### 7.4 跨核渲染

- DisplayAddon 运行在 **Core1**（避免 I2C 阻塞 Core0 的手柄输入延迟）
- 菜单状态（`ScrollWheelMenuState`）由 Core0 的 `ScrollWheelMenuAddon` 更新
- Core1 每帧快照 `volatile` 全局状态，渲染到 OLED
- 字体坐标: `drawText(x, y)` 中 x/y 是**字符坐标**（不是像素）。字体 6×8 → 屏幕 128×64 = 21 列 × 8 行

---

## 8. Fightpad12Slim 硬件配置

### 8.1 GPIO 引脚分配

| 引脚 | 功能 | 说明 |
|------|------|------|
| GP00-GP01 | I2C0 (OLED) | SDA=GP00, SCL=GP01 |
| GP02-GP20 | 手柄按键 | UP/DOWN/LEFT/RIGHT/B1-B4/L1-L2/R1-R2/L3/R3/S1/S2/A1/A2/TURBO |
| GP21 | VBUS 检测 | ESP32 电池帧的外部供电状态 |
| GP22 | WS2812 按键灯 | 12 颗 LED，GRB 格式 |
| GP23 | 板载状态 LED | 模式指示 |
| GP24 | 5V Boost 使能 | LED 供电轨 |
| GP25-GP27 | BQ27220 电池 | SCL/SDA/GPOUT (软件 I2C)，OLED 与 ESP32 共用 SOC |
| GP28-GP29 | PIO-USB 扩展 | USB 设备直通 |
| **GP30** | **编码器按键 + DIP ON/OFF** | 长短按复用 |
| **GP31** | **编码器 A 相 + DIP PREV** | 旋转+效果切换复用 |
| **GP32** | **编码器 B 相 + DIP NEXT** | 旋转+效果切换复用 |
| GP33 | HID 传输选择 | USB/BT 切换 SPDT |
| GP34-GP35 | ESP32-C6 控制 | EN/boot |
| GP40 | WS2812 环境灯 | 19 颗底部 LED |
| GP41 | 电池采样 | VBAT ADC |
| GP42-GP43/46-47 | 预留 | |
| GP44-GP45 | ESP32-C6 UART0 | TX/RX 桥接 |

### 8.2 LED 双链

```
GP40 → PIO2 SM0 → frame[19]    底部环境灯（NeoPico）
GP22 → PIO2 SM1 → frame_gp22[12] 按键灯（NeoPico）
```

环境灯默认呼吸效果（8 种颜色循环），亮度 25%。按键灯在有按键按下时闪烁白色（80ms）。

---

## 9. Fightpad12Slim 自定义 Addon 详解

### 9.1 ScrollWheelMenuAddon（拨轮菜单）

**硬件**: 旋转编码器 (GP30=SW, GP31=A相, GP32=B相) + GP19=返回键

**GP30 5 状态按键 FSM**（经典 0x1abin/MultiButton 模式）:

```
                    ┌──────────────────┐
       按下         │                  │  释放
IDLE ─────→ DEBOUNCE_PRESS ──30ms──→ PRESS ─────→ DEBOUNCE_RELEASE ──30ms──→ IDLE
                    │ bounce           │  3s 长按             ↑
                    └──→ IDLE          └────→ LONG ──────────┘
                                              navToggle()
```

- 长按 3s: 进入/退出菜单
- 短按: 选择/应用
- 旋转: 导航上下
- GP19: 返回上一级

**菜单树**（9 项）:

```
Level 0: MAIN
  ├─ RP2350B FW Version ──→ INFO (静态页)
  ├─ ESP32C6 Status     ──→ INFO (静态页)
  └─ RGB Customize      ──→ Level 1
        ├─ Top Board RGB   ──→ Level 2 (COLOR: OFF/Red/Orange/Yellow/Green/Cyan/Blue/Purple/White)
        ├─ Bottom Board RGB──→ Level 2
        ├─ Button RGB      ──→ Level 2
        └─ RGB OFF         ──→ 立即熄灭全部（不进入子菜单）
```

**跨核通信标志**:

| 变量 | 写入者 | 读取者 | 作用 |
|------|--------|--------|------|
| `g_scrollWheelMenuActive` | Core0 | Core1 DisplayAddon, Core0 AmbientLED | 菜单激活→OLED接管 + 暂停DIP |
| `g_scrollWheelButtonBusy` | Core0 | Core0 AmbientLED | GP30按下→抑制DIP（已被 `g_scrollWheelButtonLongPressed` 替代） |
| `g_scrollWheelButtonLongPressed` | Core0 | Core0 AmbientLED | 长按已触发→抑制释放边沿LED切换 |
| `g_menuRgbTop/Bottom/Button` | Core0 | Core0 AmbientLED | 菜单选择的颜色覆盖值 |
| `g_menuRgbTarget` | Core0 | Core0 | 当前 COLOR 层级中正在配置的目标 |

### 9.2 FightpadAmbientLEDAddon（环境灯）

**硬件**: GP40 底灯 (19 LED) + GP22 按键灯 (12 LED)

**DIP 控制模式**: 3 个引脚 (GP30=ON/OFF, GP31=PREV, GP32=NEXT) 低电平有效

**渲染管线**:

```
process() 每帧:
  [1] updateButtonFlash()            ← 检测手柄按键 → 80ms 白色闪光
  [2] 检查 g_scrollWheelMenuActive   ← 菜单激活 → 跳过 DIP 但保持渲染
  [3] handleControlEdges()           ← DIP 边沿检测 → LED 开关/效果切换
  [4] render() + show()              ← 20ms 更新间隔

render():
  ├─ 读 g_menuRgbBottom → bottomColor (0xFF=默认循环, 0=OFF, 1-8=Red..White)
  ├─ 读 g_menuRgbTop    → topColor
  ├─ 读 g_menuRgbButton → flashColor
  ├─ bottomColor + 呼吸亮度 → frame[19]    (GP40)
  └─ topColor + 呼吸亮度    → frame_gp22[12] (GP22, 闪光覆盖)
     若 gp22FlashUntil[led] > now → 显示 flashColor (按键闪灯)
```

**效果**: 默认 8 种颜色（Red/Orange/Yellow/LimeGreen/Green/Aqua/Blue/Purple）正弦波呼吸循环。

**仲裁机制**: GP30 被 `ScrollWheelMenuAddon` 和 `FightpadAmbientLEDAddon` 共享：
- 正常模式: GP30 短按 → LED ON/OFF 切换
- 菜单激活模式: GP30 归菜单使用，DIP 暂停
- 长按进入/退出菜单时: 抑制 LED 误切换（`g_scrollWheelButtonLongPressed` 标志位）

### 9.3 FightpadESP32ProxyAddon（ESP32-C6 代理）

**硬件**: RP2350 UART0 (GP44=TX, GP45=RX, 115200 baud)

**功能**:
- 检测 GP33 传输选择开关（USB/BT）
- 发送紧凑手柄输入帧（10ms 间隔）
- 发送电池状态帧
- 发送传输模式帧
- 电量百分比复用 OLED 的 BQ27220 SOC；VBUS 与 GP41 ADC 仅作附加诊断

**帧格式**: 魔数头 + 类型 + Payload（自定义紧凑二进制协议）

---

## 10. 构建系统

### 10.1 配置命令

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

输出: `GP2040-CE_<version>_Fightpad12Slim.uf2`

### 10.2 板级配置链路

```
GP2040_BOARDCONFIG=Fightpad12Slim
  └─ configs/Fightpad12Slim/Fightpad12Slim.cmake (设置 PICO_BOARD / PICO_PLATFORM)
       └─ configs/Fightpad12Slim/BoardConfig.h (引脚映射 + 功能宏)
            └─ configs/Fightpad12Slim/fightpad12slim.h (引脚别名常量)
```

### 10.3 代码生成

- **Protobuf**: Python venv + `nanopb_generator.py` → `config.pb.c/h` + `enums.pb.c/h`
- **版本号**: `git describe` → `configure_file(version.h.in)` → `GP2040VERSION` 等宏
- **Web 前端**: `npm ci && npm run build` → 编译产物嵌入固件

---

## 11. 开发状态

| 步骤 | 描述 | 状态 |
|------|------|:--:|
| S1-A~E | 拨轮菜单系统初始化 | pending |
| S2-A | 修复长按 GP30 进菜单时 LED 误切换 | ✅ |
| S3-A | INFO 页禁用旋转 + COLOR 终端层级 | ✅ |
| S4-A/B | RGB 颜色控制（菜单选色覆盖呼吸灯） | ✅ |
| S5-A | RGB OFF 功能 | ✅ |
| S6-A | GP19 BACK 按钮返回 | ✅ |

---

## 12. 技术要点备忘

1. **关键约束**: 英文代码/注释，OLED 不显示顶部标题栏，颜色控制暂不持久化
2. **GPIO 共享**: GP30-32 通过全局 volatile 标志位仲裁，每次只一个 Addon 使用
3. **Flash 写入**: 50ms 防抖 → 全扇区擦写 → 无磨损均衡。RGB 颜色持久化预留方案见 `.emv2/memory-log.md`
4. **Addon 编译**: 所有 Addon 无条件编译，通过 `available()` 运行时自选。死代码由 `-Wl,--gc-sections` 去除
5. **跨核通信**: 通过 `volatile` 全局变量 + Storage 单例共享内存。Core0 写入菜单状态 → Core1 每帧快照渲染

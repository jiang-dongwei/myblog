---
name: gp2040-ce-project-context
description: GP2040-CE Fightpad12Slim 项目完整上下文 — 架构、已完成步骤、待讨论事项
metadata:
  type: project
---

# GP2040-CE Fightpad12Slim — 项目上下文

## 项目基础

- **固件**: GP2040-CE v0.7.12，开源格斗手柄固件
- **开发板**: SparkFun Pro Micro RP2350 (RP2350B，双核 Cortex-M33，150MHz)
- **硬件**: 自研 Fightpad12Slim 格斗手柄
- **BoardConfig**: `Fightpad12Slim`
- **编译**: CMake + Ninja，GCC 14.2.1，`rp2350-arm-s` 平台
- **语言**: C++17，英文代码/注释
- **工作区路径**: `e:\ComporyProject\aa\GP2040-CE`
- **构建目录**: `build/`（当前可用）
- **项目文档**: `docs/ARCHITECTURE.md`（刚写的完整架构分析）

## 硬件引脚分配（关键引脚）

| 引脚 | 功能 |
|------|------|
| GP00-01 | I2C0 OLED (SDA=0, SCL=1) |
| GP02-20 | 手柄按键 (UP/DOWN/LEFT/RIGHT/B1-B4/L1-R2/L3/R3/S1/S2/A1/A2/TURBO) |
| GP19 | BACK 按钮（菜单返回），兼 `BUTTON_PRESS_A2` |
| GP22 | WS2812 按键灯 12 颗（顶板），GRB 格式 |
| GP23 | 板载状态 LED |
| GP24 | 5V Boost 使能 |
| GP25-27 | BQ27220 电池电量计 |
| GP28-29 | PIO-USB 扩展 |
| GP30 | 编码器按键 (SW) + 环境灯 ON/OFF DIP — **双功能共享** |
| GP31 | 编码器 A 相 + 环境灯 PREV DIP |
| GP32 | 编码器 B 相 + 环境灯 NEXT DIP |
| GP33 | HID 传输选择开关 (USB/BT) |
| GP40 | WS2812 环境灯 19 颗（底板），GRB 格式 |
| GP41 | 电池 ADC 采样 |
| GP44-45 | ESP32-C6 UART0 桥接 |

## 双核架构

- **Core0** (`gp2040.cpp`): GPIO 扫描 → 去抖 → GamepadState → SOCD 清洗 → 热键 → USB HID 发送
- **Core1** (`gp2040aux.cpp`): 显示渲染 (DisplayAddon)、电池、LED 动画

## 自定义 Addon（Fightpad12Slim 特有）

### ScrollWheelMenuAddon (Core0)
- 拨轮编码器菜单系统
- GP30: 5 状态 FSM（短按=选择，长按 3s=进入/退出菜单）
- GP31/GP32: 旋转编码器导航
- GP19: BACK 返回上一级
- 菜单树: MAIN → RGB_SUB → COLOR（9 项：OFF/Red/Orange/Yellow/Green/Cyan/Blue/Purple/White）
- RGB_SUB 有第 4 项 "RGB OFF" 即时全部熄灭
- INFO 页（RP2350 FW / ESP32 C6）显示 "Coming soon"，禁用旋转

### FightpadAmbientLEDAddon (Core0)
- GP40 底灯 19 颗 (frame[])，GP22 按键灯 12 颗 (frame_gp22[])
- DIP 控制: GP30 ON/OFF, GP31 PREV, GP32 NEXT（低电平有效）
- 默认 8 色呼吸循环 (Red/Orange/Yellow/LimeGreen/Green/Aqua/Blue/Purple)
- 按键闪灯: 80ms 白色闪光
- 菜单激活时 DIP 暂停（`g_scrollWheelMenuActive` 标志）
- 长按不会误触发 LED 切换（`g_scrollWheelButtonLongPressed` 标志）

### FightpadESP32ProxyAddon (Core0)
- UART0 GP44/GP45 @115200 与 ESP32-C6 通信
- 发送手柄输入帧、电池状态帧、传输模式帧

## 跨核通信标志（重要）

| 变量 | 位置 | 作用 |
|------|------|------|
| `g_scrollWheelMenuActive` | scrollwheel_menu.cpp | 菜单激活 → OLED 接管 + DIP 暂停 |
| `g_scrollWheelButtonBusy` | scrollwheel_menu.cpp | GP30 按下即设 true |
| `g_scrollWheelButtonLongPressed` | scrollwheel_menu.cpp | 长按已触发 → 抑制释放边沿 LED 切换 |
| `g_menuRgbTop` (0xFF=未设, 0-15=AS颜色索引) | scrollwheel_menu.cpp | 顶板按键灯颜色覆盖 |
| `g_menuRgbBottom` (同上) | scrollwheel_menu.cpp | 底板环境灯颜色覆盖 |
| `g_menuRgbButton` (同上) | scrollwheel_menu.cpp | 按键闪灯颜色覆盖 |
| `g_menuRgbTarget` (0/1/2) | scrollwheel_menu.cpp | COLOR 层级当前配置目标 |
| `g_menuState` | scrollwheel_menu.cpp | 菜单状态（Core0 写，Core1 读） |

## 存储系统

- **FlashPROM**: 32KB @ 0x101F8000，`writeCache[0x8000]` RAM 缓存
  - `start()`: Flash → RAM
  - `commit()`: 50ms 防抖后全扇区擦写
- **Storage**: Meyer's 单例，持有 Config protobuf
  - `ConfigUtils::load(save)`: 读/写 protobuf Config 到 EEPROM
  - Footer 格式: `[padding...][protobuf data][Footer{dSize, crc32, magic=0xd2f1e365}]`
- **Proto 文件**:
  - `proto/enums.proto` → `enums.pb.h` (43 个枚举)
  - `proto/config.proto` → `config.pb.h/c` (15 个 message)
- **WebConfig**: 内嵌 lwIP HTTPD + React SPA

## 已完成的开发步骤

| 步骤 | 描述 | 状态 |
|------|------|:--:|
| S1-A~E | 拨轮菜单系统初始化 | pending（实际已完成） |
| S2-A | 修复长按 GP30 进菜单时 LED 误切换 | ✅ |
| S3-A | INFO 页禁用旋转 + COLOR 终端层级 | ✅ |
| S4-A | 全局颜色状态变量 + COLOR 层级短按写入 | ✅ |
| S4-B | render() 使用菜单颜色覆盖 + 按钮闪灯颜色 | ✅ |
| S5-A | RGB OFF 功能 | ✅ |
| S6-A | GP19 BACK 按钮返回 | ✅ |
| S7-A | g_menuRgb* 对齐 AnimationStation colors 索引 (0-15) | ✅ |
| S7-B | FightpadAmbientLEDOptions protobuf message + Flash 持久化 | ✅ |
| S7-C | 移除 DIP GPIO31/32 直接切换颜色 | ✅ |

## 已取消的功能

- **DIP 直接切换颜色 (GPIO31/32)**: 已从 `FightpadAmbientLEDAddon::process()` 移除。颜色现在仅通过拨轮菜单设置。GPIO30-32 专属于 ScrollWheelMenuAddon。

## 待讨论/未实施

1. ~~**RGB 颜色持久化 (写 Flash)**~~: ✅ 已完成，方案 A — `config.proto` 新增 `FightpadAmbientLEDOptions`
   - 方案 A: 改 `proto/config.proto` 新增 `FightpadAmbientLEDOptions`，走 protobuf → `ConfigUtils::save()`
   - 方案 B: 直接 `EEPROM.writeCache[0x7FF0]=magic;` 偷 4 字节，调 `EEPROM.commit()`
   - 用户尚未决定

2. **RP2350 Firmware Version 信息页**: `version.h` 已有数据 (`GP2040VERSION`, `GP2040BUILD`)，OLED 还显示 "Coming soon"

3. **ESP32C6 Status 信息页**: 无回读机制，当前 UART 只发送 (RP2350→ESP32)

4. **AnimationStation 系统**: 有 6 种效果（Rainbow/Chase/StaticTheme/CustomTheme/StaticColor/CustomThemePressed），但 FightpadAmbientLEDAddon 不经过这个系统

## 相关讨论记录

- `.emv2/discussion/2026-07-06-scrollwheel-menu/` — 拨轮菜单系统原始设计
- `.emv2/discussion/2026-07-07-longpress-no-led-toggle/` — 长按 LED 误切换根因
- `.emv2/discussion/2026-07-07-menu-nav-fixes/` — INFO 旋转禁用 + COLOR 返回
- `.emv2/discussion/2026-07-07-rgb-color-control/` — RGB 颜色控制
- `.emv2/discussion/2026-07-07-rgb-off/` — RGB OFF 功能

## 关键约束

- 英文代码/注释
- OLED 不显示顶部标题栏
- COLOR 层级短按 → 应用颜色 + 停留在当前页（不返回上层）
- 菜单退出通过长按 GP30 或 GP19 BACK
- GP30 活跃低电平 (ACTIVE_LOW=1)

## 架构文档

完整架构文档: [[ARCHITECTURE]] — 见 `docs/ARCHITECTURE.md`

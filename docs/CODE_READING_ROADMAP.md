# Fightpad12Slim RP2350 源码阅读路线

## 目标

这份路线用于逐步看懂当前仓库中的 GP2040-CE / Fightpad12Slim 固件。

阅读时始终围绕一条真实数据链：

```text
物理按键
  -> RP2350 GPIO
  -> 去抖后的 GPIO 位图
  -> GamepadState
  -> Addon 修改
  -> USB 或 ESP32-C6 报告
  -> PC / 主机看到手柄输入
```

不要一开始通读 `lib/`、所有 `drivers/`、`www/` 或整个 BQ27220 驱动。这些内容体量大，但不是理解主程序的前置条件。

## 总体划分

| 阶段 | 主题 | 学完后应能回答 |
|---|---|---|
| 1 | 板级配置与编译边界 | 为什么当前固件是 Fightpad12Slim + RP2350，而不是其他 GP2040-CE 开发板？ |
| 2 | 启动流程与双核分工 | 上电后 Core0、Core1 分别从哪里开始运行？ |
| 3 | 按键输入与 Gamepad 数据 | GP6 按下后，怎样变成 `B1` 按键状态？ |
| 4 | Addon 插件管线 | `preprocess()`、`process()`、`postprocess()` 分别在什么时候运行？ |
| 5 | USB 输出与协议驱动 | 同一个 `GamepadState` 怎样转换成 XInput、Switch 或 PS 模式报告？ |
| 6 | 拨轮菜单与 OLED | GP30 长按后，Core0 的菜单状态怎样显示到 Core1 的 OLED？ |
| 7 | RGB、电量与电源优先级 | 菜单灯效、蓝牙临时灯效、低电关灯和 GP24 电源如何仲裁？ |
| 8 | RP2350 与 ESP32-C6 协同 | RP2350 通过 UART 给 ESP32 发送什么，又从 ESP32 接收什么？ |
| 9 | 配置存储、WebConfig 与扩展模块 | 菜单和网页修改的配置怎样保存到 Flash，并在重启后恢复？ |

---

## 阶段 1：板级配置与编译边界

### 阅读顺序

1. `CMakeLists.txt`
   - 只先看顶部的 `PICO_BOARD`、`PICO_PLATFORM`、`GP2040_BOARDCONFIG`。
   - 暂时跳过依赖下载、Web 前端和全部源文件列表。
2. `configs/Fightpad12Slim/Fightpad12Slim.cmake`
   - 看 Fightpad12Slim 如何选中 `fightpad12slim` 和 `rp2350-arm-s`。
3. `configs/Fightpad12Slim/fightpad12slim.h`
   - 看 Pico SDK 认识这块 RP2350B 板所需的底层板信息。
4. `configs/Fightpad12Slim/BoardConfig.h`
   - 重点看 GP2～GP20 的普通按键映射。
   - 再看标成 `ASSIGNED_TO_ADDON` 的 GPIO。
   - 最后看 OLED、RGB、BQ27220、ESP32 proxy 等功能宏。

### 重点概念

- `PICO_BOARD`：Pico SDK 认识的物理开发板。
- `PICO_PLATFORM`：当前是 RP2350 的 Arm Secure 构建平台。
- `GP2040_BOARDCONFIG`：GP2040-CE 自己的产品级配置。
- `GPIO_PIN_xx > 0` 的普通按键由主输入扫描处理。
- `ASSIGNED_TO_ADDON` 表示该 GPIO 不进入普通按键扫描，由某个 Addon 自己管理。

### 本阶段练习

从 `BoardConfig.h` 找出：

- GP6 为什么是 `B1`。
- GP22 为什么不作为普通按键处理。
- GP30、GP31、GP32 为什么能组成拨轮菜单输入。
- GP44、GP45 为什么属于 ESP32-C6 通信。

---

## 阶段 2：启动流程与双核分工

### 阅读顺序

1. `src/main.cpp`
2. `headers/gp2040.h` 与 `src/gp2040.cpp`
   - 第一遍只看 `GP2040::setup()` 和 `GP2040::run()`。
3. `headers/gp2040aux.h` 与 `src/gp2040aux.cpp`
   - 只看 `GP2040Aux::setup()` 和 `GP2040Aux::run()`。

### 主调用链

```text
main()
  -> new GP2040()
  -> new GP2040Aux()
  -> GP2040::setup()
  -> multicore_launch_core1(core1)
       -> GP2040Aux::setup()
       -> GP2040Aux::run()
  -> 等待 Core1 ready
  -> GP2040::run()
```

### 双核分工

- Core0：GPIO 输入、Gamepad 处理、Addon 输入加工、USB 报告、ESP32 输入帧。
- Core1：OLED、电池、普通指示灯、蜂鸣器、震动等较慢的外设任务。

### 本阶段练习

在 `GP2040::setup()` 和 `GP2040Aux::setup()` 中分别列出实际加载的 Addon，解释为什么 OLED 和 BQ27220 放在 Core1，而拨轮输入和 ESP32 proxy 放在 Core0。

---

## 阶段 3：按键输入与 Gamepad 数据

### 阅读顺序

1. `headers/gamepad/GamepadState.h`
2. `headers/gamepad.h`
3. `src/gp2040.cpp`
   - `initializeStandardGpio()`
   - `debounceGpioGetAll()`
   - `GP2040::run()`
4. `src/storagemanager.cpp`
   - `setFunctionalPinMappings()`
5. `src/gamepad.cpp`
   - `Gamepad::read()`
   - `Gamepad::process()`
   - `Gamepad::hotkey()`

### 主数据链

```text
gpio_get_all()
  -> 取反得到低电平有效按键
  -> debounceGpioGetAll()
  -> gamepad->debouncedGpio
  -> Gamepad::read()
  -> GamepadState.dpad / buttons / axes
  -> Gamepad::process()
  -> 方向反转、4-Way、SOCD、摇杆转换
```

### 本阶段练习

完整追踪 GP6：

```text
BoardConfig GPIO_PIN_06
  -> functionalPinMappings[6]
  -> buttonGpios bit 6
  -> debouncedGpio bit 6
  -> mapButtonB1
  -> GamepadState.buttons 中的 GAMEPAD_MASK_B1
```

能独立讲清这条链，才进入下一阶段。

---

## 阶段 4：Addon 插件管线

### 阅读顺序

1. `headers/gpaddon.h`
2. `headers/addonmanager.h`
3. `src/addonmanager.cpp`
4. 回到 `GP2040::setup()` 和 `GP2040Aux::setup()` 看加载顺序。
5. 选一个较小模块作为例子：
   - `src/addons/reverse.cpp`
   - `src/addons/turbo.cpp`

### 生命周期

```text
LoadAddon()
  -> available()
  -> setup()

每轮主循环：
  -> PreprocessAddons()
  -> Gamepad::process()
  -> ProcessAddons()
  -> USB 驱动 process()
  -> PostprocessAddons()
```

### 重点概念

- 源文件被编译，不代表功能一定运行。
- `available()` 决定 Addon 是否进入当前运行实例。
- Addon 加载顺序会影响多个模块修改同一份 `GamepadState` 时的最终结果。
- Core0 和 Core1 各有一个独立的 `AddonManager`。

### 本阶段练习

解释 Turbo 为什么靠近 Core0 Addon 队列尾部，以及一个 Addon 若在 `postprocess()` 发送数据时，拿到的是哪个处理阶段的状态。

---

## 阶段 5：USB 输出与协议驱动

### 阅读顺序

1. `headers/gpdriver.h`
2. `src/drivermanager.cpp`
3. 先只选择一个简单代表：
   - `headers/drivers/xinput/XInputDriver.h`
   - `src/drivers/xinput/XInputDriver.cpp`
4. 再看：
   - `src/usbdriver.cpp`
   - TinyUSB 相关回调

### 主调用链

```text
启动选择 InputMode
  -> DriverManager::setup()
  -> 创建对应 GPDriver
  -> driver->initialize()

Core0 每轮：
  -> inputDriver->process(gamepad)
  -> 填充该协议的 report
  -> TinyUSB 发送
  -> tud_task()
```

### 阅读边界

先理解 XInput 一种驱动即可。Switch、PS4、PS5、Xbox One 的描述符、认证和加密流程放到最后按需学习。

### 本阶段练习

继续追踪阶段 3 的 `B1`，找到它最终进入 XInput 报告的字段，并说明 USB 未发送成功时主循环是否停止。

---

## 阶段 6：拨轮菜单与 OLED

### 阅读顺序

1. `headers/addons/scrollwheel_menu.h`
2. `src/addons/scrollwheel_menu.cpp`
   - `setup()`
   - GP30 按键状态机
   - 编码器处理
   - `navToggle()`、`navSelect()`、`navBack()`
   - `process()`
3. `src/addons/display.cpp`
   - `setup()`
   - `process()`
   - 拨轮菜单和蓝牙状态覆盖路径
4. `docs/FIGHTPAD_MENU_RGB_SUBSYSTEM_GUIDE.md`

### 跨核关系

```text
Core0 ScrollWheelMenuAddon
  -> 读取 GP30/31/32
  -> 更新菜单状态和游戏输入锁状态
  -> 发布共享状态

Core1 DisplayAddon
  -> 读取菜单状态快照
  -> 绘制到 OLED
```

### 本阶段练习

解释：

- GP30 短按和长按如何互斥。
- 当前长按阈值为什么是 2000 ms。
- 菜单打开后，为什么 GP19 仍能返回，但不能继续作为游戏按键输出。
- 退出菜单后为什么还要等待游戏按键释放。

---

## 阶段 7：RGB、电量与电源优先级

### 阅读顺序

1. `headers/addons/fightpad_ambient_leds.h`
2. `src/addons/fightpad_ambient_leds.cpp`
   - `process()`
   - `render()`
   - `renderAmbient()`
   - `renderButtons()`
   - `show()`
   - `setBoostPower()`
3. `headers/addons/fightpad_bq27220_battery.h`
4. `src/addons/fightpad_bq27220_battery.cpp`
   - 第一遍只看 `available()`、`setup()`、`process()` 和共享快照接口。
   - 暂时跳过底层 bit-bang I2C 与全部 Data Memory 配置细节。
5. `docs/BQ27220_BATTERY_GAUGE_GUIDE.md`

### 重点关系

```text
菜单持久化灯效
  + 按键 Flash
  + 蓝牙状态临时 Base 灯效
  + BQ27220 低电保护
  -> 最终 GP22 / GP40 帧
  -> GP24 RGB 电源门控
```

### 本阶段练习

解释以下优先级为什么不能颠倒：

- `SOC <= 7%` 必须能压住所有临时灯效。
- All OFF 需要先发送黑帧，再处理 GP24。
- 蓝牙 Pairing/Connecting 临时效果不能改写用户保存的 RGB 配置。

---

## 阶段 8：RP2350 与 ESP32-C6 协同

### 阅读顺序

1. `headers/addons/fightpad_esp32_proxy.h`
2. `src/addons/fightpad_esp32_proxy.cpp`
   - `available()`、`setup()`
   - `process()` 与 `postprocess()`
   - 8 字节接收状态机
   - `sendTransportModeFrame()`
   - `sendBatteryStatusFrame()`
   - `sendInputReportFrame()`
3. 协议文档：
   - `docs/rp2350_input_report_protocol_esp32.md`
   - `docs/fw_info_protocol_rp2350.md`
   - `docs/bluetooth_status_protocol_rp2350.md`
   - `docs/menu_gameplay_lock_protocol_rp2350_esp32.md`

### 帧类型

统一帧长度为 8 字节，当前源码中的第二个魔数用于区分：

- `FP`：输入报告。
- `FT`：传输模式。
- `FB`：电池状态。
- `FI`：ESP32 固件信息。
- `FS`：蓝牙连接状态。

### 阅读边界

这一阶段只学习本仓库的 RP2350 端。ESP32-C6 的 BLE HID、休眠和重连逻辑属于另一个工程，不能从本仓库的接收端代码反推为已经实现。

### 本阶段练习

从一个按键变化开始，追踪到 `FP` 帧写入 UART；再从一帧 `FS` 数据开始，追踪到 OLED 状态页和 GP40 蓝牙灯效。

---

## 阶段 9：配置存储、WebConfig 与扩展模块

### 阅读顺序

1. `headers/storagemanager.h`
2. `src/storagemanager.cpp`
3. `proto/config.proto`
   - 先看顶层 `Config`，不要从第一行顺序读完整文件。
4. `src/config_utils.cpp`
   - 重点看默认值填充、序列化和反序列化入口。
5. `src/webconfig.cpp`
6. `www/`

### 主数据链

```text
BoardConfig 编译期默认值
  -> ConfigUtils 填充未设置字段
  -> Storage 中的 Config
  -> 菜单 / WebConfig 修改
  -> protobuf 序列化
  -> FlashPROM
  -> 下次启动恢复
```

### 最后再看的内容

- `src/drivers/` 中未使用的其他 USB 协议。
- `src/usbhostmanager.cpp` 和 USB 认证流程。
- `src/animationstation/` 通用动画框架。
- `lib/` 第三方库内部实现。
- `www/` React 页面细节。

这些内容按需求选读，不是理解 Fightpad12Slim 主运行链的必修前置。

---

## 每一阶段的协作方式

逐阶段学习时，每一阶段都按同一格式进行：

1. 先画出本阶段最小调用链。
2. 说明每个文件的职责，不一开始逐行翻译。
3. 选一条真实输入或硬件行为逐函数跟踪。
4. 对关键结构和条件分支做片段级解释。
5. 最后由学习者复述“输入、状态变化、输出”。
6. 确认能讲清后，再进入下一阶段。

第一轮从“阶段 1：板级配置与编译边界”开始。

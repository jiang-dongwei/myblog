# ESP32-C6 BLE HID Gamepad 项目接手指南

> 适用目录：`esp32c6_ble_hid_gamepad_test`
>
> 本文只描述 ESP32-C6 固件。内容依据当前 `main/main.c`、CMake 配置、
> `sdkconfig.defaults` 和现有构建产物整理。

## 1. 项目概览

这是 Fightpad 12 Slim 上 ESP32-C6 的独立 BLE HID 固件。它不直接扫描实体按键，
而是从 UART 接收外部输入状态，将其转换为标准蓝牙游戏手柄报告并发送给主机。

当前固件信息约定如下：

| 字段 | 值 |
| --- | --- |
| SDK | ESP-IDF |
| CPU | ESP32-C6，单核 RISC-V，当前配置 160 MHz |
| Plat | `esp32c6` |
| Board/构建目标 | `fightpad12slim_c6_ble_hid` |
| BLE 设备名 | `FP12Slim-C6` |
| Flash | 4 MB |
| 工程类型 | ESP-IDF Minimal Build，Single App 分区 |

工程当前承担四项主要职责：

1. 解析 UART0 上的 8 字节二进制帧。
2. 根据传输模式开启或关闭蓝牙功能。
3. 管理 BLE HID、配对窗口和连接状态。
4. 生成并发送游戏手柄报告及电量信息。

明确不属于当前工程的功能：

- 实体按键、摇杆或方向键的 GPIO 扫描。
- USB HID 设备实现。
- Wi-Fi、OTA 和网络配置。
- 电池 ADC/电量计的直接采样。
- 显示屏、灯效和用户配置界面。

## 2. 目录结构

```text
esp32c6_ble_hid_gamepad_test/
├── CMakeLists.txt              ESP-IDF 工程入口和工程名
├── README.md                   最初的运行、UART 和烧录说明
├── sdkconfig.defaults          目标芯片、BLE HID、Flash 等默认配置
├── sdkconfig                   当前生成的完整 ESP-IDF 配置
├── main/
│   ├── CMakeLists.txt          main 组件及依赖声明
│   └── main.c                  全部应用逻辑
└── build/                      现有编译产物和中间文件
```

工程规模很小，应用层目前没有拆分模块；阅读和修改的主要入口是 `main/main.c`。

## 3. 构建依赖

根目录 `CMakeLists.txt` 定义：

```cmake
idf_build_set_property(MINIMAL_BUILD ON)
project(fightpad12slim_c6_ble_hid)
```

`main/CMakeLists.txt` 注册 `main.c`，组件依赖如下：

| 组件 | 用途 |
| --- | --- |
| `esp_hid` | ESP-IDF HID Device 抽象层 |
| `bt` | BLE Controller 和 NimBLE Host |
| `esp_driver_uart` | UART0 数据收发 |
| `nvs_flash` | NVS 初始化及蓝牙相关存储基础 |
| `esp_driver_gpio` | GPIO13 配对按键 |

`sdkconfig.defaults` 中的重要选项：

- 目标芯片：ESP32-C6。
- 启用 Bluetooth 和 NimBLE。
- 启用 NimBLE HID Service。
- 启用 Battery Service 及电量通知。
- 最大 BLE 连接数为 1。
- 最大绑定数为 4。
- Flash 为 4 MB。
- 使用 Single App 分区表。

当前完整 `sdkconfig` 还显示：

- CPU 默认频率为 160 MHz。
- ESP-IDF Console 使用 UART0，115200 baud。
- `CONFIG_BT_NIMBLE_NVS_PERSIST` 未启用，绑定跨重启的持久化行为需要实机确认。

## 4. 总体架构

```text
                 UART0 RX: GPIO17
外部状态源  ─────────────────────────►  uart_input_task
                                           │
                                           ▼
                                  8 字节帧解析与校验
                                           │
                 ┌─────────────────────────┼─────────────────────────┐
                 │                         │                         │
                 ▼                         ▼                         ▼
          Gamepad Report             Transport Mode              Battery
          更新当前报告                开关 BLE 通道              更新 BAS 电量
                 │                         │
                 ▼                         ▼
           report_task              广播/断开连接控制
                 │
                 ▼
          ESP HID Device API
                 │
                 ▼
          NimBLE GATT/HID Service
                 │
                 ▼
             BLE 主机设备

GPIO13 ─► pair_button_task ─► 60 秒配对窗口、清除旧绑定、启动广播
```

代码同时使用两层蓝牙接口：

- `esp_hidd_*`：建立 HID 服务并发送输入报告。
- NimBLE GAP API：控制广播、连接、断开、配对和绑定记录。

## 5. 启动流程

入口函数为 `app_main()`，实际顺序如下：

1. 调用 `nvs_flash_init()` 初始化 NVS。
2. 如果 NVS 页损坏或版本不兼容，则擦除并重新初始化。
3. 创建 ESP-IDF 默认事件循环。
4. 释放 Classic Bluetooth 内存，只保留 BLE。
5. 初始化并启用 ESP32-C6 BLE Controller。
6. 初始化 NimBLE。
7. 将 GPIO13 配置为带内部上拉的输入，用作配对按键。
8. 初始化 UART0：GPIO16 TX、GPIO17 RX、115200 8N1。
9. 启动 UART 接收任务。
10. 启动一次性 `C6_DONE` 通知任务。
11. 设置 BLE 设备名和安全参数。
12. 初始化 BLE HID Device。
13. 初始化 NimBLE Store。
14. 启动 NimBLE Host 任务。
15. 启动配对按键任务。

正常编译配置下，上电不会无条件开始广播。必须满足以下条件才能广播：

```text
HID 已启动
AND 当前传输模式允许 Bluetooth
AND 60 秒配对窗口有效
AND 当前没有连接
AND 当前没有正在广播
```

## 6. FreeRTOS 任务划分

| 任务 | 栈大小 | 优先级 | 生命周期 | 职责 |
| --- | ---: | ---: | --- | --- |
| `uart_input_task` | 3072 | 6 | 常驻 | 每次最多读取 64 字节并喂给帧解析器 |
| `hid_report_task` | 3072 | 5 | 按需启动后常驻 | 超时中立化、变化发送、50 ms 保活 |
| `pair_button_task` | 2048 | 5 | 常驻 | 按键消抖、配对窗口和广播维护 |
| `uart_done_signal_task` | 2048 | 4 | 启动时短时运行 | 重复发送 `C6_DONE\n` 后自删除 |
| NimBLE Host Task | 由组件配置 | 组件管理 | 常驻 | BLE Host 协议栈事件循环 |

`hid_report_task` 不在 `app_main()` 中立即创建，而是在 BLE HID 连接或 GATT
订阅事件到来时由 `ensure_report_task_running()` 创建。

共享状态通过三个 FreeRTOS 临界区锁保护：

- `s_report_lock`：当前 HID 报告及 UART 报告时间。
- `s_transport_lock`：传输模式状态。
- `s_pairing_lock`：配对窗口及截止时间。

## 7. UART 接口

### 7.1 硬件配置

| 项目 | 配置 |
| --- | --- |
| 控制器 | UART0 |
| TX | GPIO16 |
| RX | GPIO17 |
| 波特率 | 115200 |
| 数据格式 | 8N1，无流控 |
| RX 驱动缓冲区 | 512 字节 |

UART0 同时还是当前 ESP-IDF Console。也就是说 TX 线上会同时出现：

- ESP-IDF 启动日志和 `ESP_LOG*` 日志。
- 固件主动发送的 `C6_DONE\n`。

二进制控制帧只在 RX 方向进入 C6。与 C6 TX 相连的接收方不能假定返回流全是纯二进制协议，
必须能够忽略文本日志。

### 7.2 通用帧格式

所有输入帧固定为 8 字节：

| 字节 | 含义 |
| ---: | --- |
| 0 | 固定 `0x46`，ASCII `F` |
| 1 | 帧类型：`P`、`T` 或 `B` |
| 2～6 | 帧类型相关载荷 |
| 7 | Byte 0～6 逐字节 XOR 校验值 |

帧类型：

| Byte 1 | ASCII | 含义 |
| ---: | --- | --- |
| `0x50` | `P` | Gamepad Report |
| `0x54` | `T` | Transport Mode |
| `0x42` | `B` | Battery Status |

解析器会搜索 `0x46` 帧头，再验证第二字节是否为已知类型。收到完整 8 字节后才进行
XOR 校验。校验失败的帧会被丢弃。

### 7.3 Gamepad Report：`FP`

| 字节 | 含义 |
| ---: | --- |
| 0 | `0x46` |
| 1 | `0x50` |
| 2 | 按钮 1～8 位图 |
| 3 | 按钮 9～16 位图 |
| 4 | D-pad 位图 |
| 5 | X 轴，`int8_t`，范围 -127～127 |
| 6 | Y 轴，`int8_t`，范围 -127～127 |
| 7 | XOR 校验 |

D-pad 位定义：

| Bit | 方向 |
| ---: | --- |
| 0 | Up |
| 1 | Down |
| 2 | Left |
| 3 | Right |

相反方向同时出现时会互相抵消。例如 Up+Down 视为垂直中立；Left+Right 视为水平中立。
对角方向会映射为 HID Hat 的 1、3、5、7。

只有 Bluetooth 传输模式启用时，`FP` 帧才会更新当前报告；否则当前报告会被保持为中立值。

### 7.4 Transport Mode：`FT`

| 字节 | 含义 |
| ---: | --- |
| 0 | `0x46` |
| 1 | `0x54` |
| 2 | `0` 表示关闭 Bluetooth；非 `0` 表示启用 Bluetooth |
| 3～6 | 当前代码忽略 |
| 7 | XOR 校验 |

关闭 Bluetooth 时固件会：

1. 将当前游戏手柄报告恢复为中立值。
2. 停止正在进行的 BLE 广播。
3. 主动断开已有 BLE 连接。

启用 Bluetooth 时会尝试调用 `start_advertising()`，但广播仍要求配对窗口处于有效期。

### 7.5 Battery Status：`FB`

| 字节 | 含义 |
| ---: | --- |
| 0 | `0x46` |
| 1 | `0x42` |
| 2 | 电量百分比，直接交给 HID Battery Service |
| 3 | USB 供电标志，非零表示存在 USB 供电 |
| 4～5 | 原始电池数据，仅用于 Debug 日志 |
| 6 | 当前代码未使用 |
| 7 | XOR 校验 |

当前代码不会限制 Byte 2 的范围，发送方应保证电量值为 `0～100`。
`s_battery_usb_power_present` 目前只保存状态，没有继续参与 BLE 报告或控制逻辑。

### 7.6 启动完成通知

UART 初始化后，固件会发送：

```text
C6_DONE\n
```

发送 20 次，每次间隔 100 ms，持续约 2 秒。该通知不是 8 字节二进制帧，也没有 XOR 校验。

## 8. BLE HID 报告

HID Report ID 为 1，报告长度为 5 字节：

| 报告字节 | 内容 |
| ---: | --- |
| 0 | Button 1～8 |
| 1 | Button 9～16 |
| 2 | 低 4 位为 Hat，值 `0～7`；`8` 表示中立 |
| 3 | X 轴，8 位有符号数 |
| 4 | Y 轴，8 位有符号数 |

按钮位直接映射为 HID Button 1～16。README 中约定的前 15 位为：

```text
B1, B2, B3, B4, L1, L2, R1, R2,
L3, R3, S1, S2, A1, A2, TURBO
```

Bit 15 保留。

报告任务每 5 ms 检查一次状态：

- 与上次报告不同：立即发送。
- 状态未变化但距离上次发送达到 50 ms：发送保活报告。
- 未连接：不调用 HID 发送接口，并清除“上次已发送”状态。
- 最后一个有效 Gamepad Report 超过 250 ms：恢复中立报告，防止按键卡住。

## 9. BLE 广播、连接和配对

### 9.1 广播内容

- 完整设备名：`FP12Slim-C6`。
- Appearance：Gamepad。
- 广播 HID Service UUID：`0x1812`。
- 广播间隔：30～50 ms。
- 可连接、通用可发现模式。

### 9.2 安全设置

- IO 能力：No Input/No Output。
- 配对方式：Just Works，不需要密码。
- 启用 bonding。
- 不启用 MITM。
- 启用 Secure Connections。
- 双方分发加密密钥和身份密钥。

### 9.3 配对按键

| 项目 | 配置 |
| --- | --- |
| GPIO | GPIO13 |
| 模式 | 输入、内部上拉 |
| 扫描周期 | 10 ms |
| 消抖时间 | 30 ms |
| 配对窗口 | 60 秒 |

代码启动时读取一次 GPIO13 电平并把它作为空闲电平，之后任何相反电平都视为按下。
因此上电时不应一直按住配对键，否则空闲电平可能判断错误。

触发配对模式后：

1. 如果尚未收到 Transport Mode，默认启用 Bluetooth。
2. 打开 60 秒配对窗口。
3. 停止已有广播。
4. 断开已有连接。
5. 调用 `ble_store_clear()` 清除绑定记录。
6. 重新开始广播。

当前 `pair_button_task()` 在稳定状态从松开变按下、以及从按下变松开时都会调用
`trigger_pairing_mode()`。松开时通常只会检测到配对窗口已经打开并确保广播继续，
但如果今后修改配对逻辑，需要注意这个“按下和释放均触发”的行为。

### 9.4 连接事件

- 连接成功：记录 connection handle 并关闭配对窗口。
- 连接失败：清除 handle 并尝试重新广播。
- 断开连接：清除 handle 并尝试重新广播。
- 重复配对：删除对应旧 Peer 并重试配对。
- HID 连接或 GATT Subscribe：确保报告任务已启动。

由于 `start_advertising()` 强制要求配对窗口有效，而连接成功后又会关闭配对窗口，
因此断线后的自动重新广播实际上会被配对窗口条件挡住。断线重连策略应作为实机重点验证项。

## 10. 关键状态变量

| 状态 | 作用 |
| --- | --- |
| `s_hid_dev` | ESP HID Device 实例 |
| `s_hid_started` | HID Service 是否完成启动 |
| `s_conn_handle` | 当前 NimBLE 连接句柄 |
| `s_transport_mode_seen` | 是否至少收到过一次 Transport Mode |
| `s_transport_bt_enabled` | 当前是否允许 BLE 通道 |
| `s_pairing_window_open` | 配对窗口标志 |
| `s_pairing_window_deadline` | 配对窗口截止 tick |
| `s_current_report` | 当前 5 字节 HID 报告 |
| `s_last_uart_tick` | 最后一次有效 Gamepad Report 的 tick |
| `s_have_uart_report` | 当前报告是否来自有效 UART 输入 |
| `s_battery_level` | 当前电量，初始为 100% |
| `s_battery_usb_power_present` | UART 上报的 USB 供电状态，当前未进一步使用 |

## 11. 关键函数索引

| 函数 | 职责 |
| --- | --- |
| `app_main()` | 总初始化入口 |
| `init_nimble_controller()` | BLE Controller 和 NimBLE 初始化 |
| `configure_security()` | Just Works、bonding 和密钥策略 |
| `hidd_event_callback()` | ESP HID Device 事件 |
| `gap_event()` | NimBLE GAP 连接、订阅、加密和配对事件 |
| `start_advertising()` | 检查条件、组装广播数据并开始广播 |
| `pair_button_task()` | 配对键消抖、窗口超时和广播维护 |
| `trigger_pairing_mode()` | 清绑定、断开连接并开启配对窗口 |
| `init_input_uart()` | 配置 UART0 和启动接收任务 |
| `parse_uart_byte()` | 流式帧同步和组帧 |
| `handle_uart_frame()` | 校验并分发 `FP`、`FT`、`FB` 帧 |
| `compose_hid_report()` | UART 状态转为 5 字节 HID 报告 |
| `report_task()` | HID 变化发送、保活和超时中立化 |
| `update_hid_battery_level()` | 更新 Battery Service 电量 |

## 12. 构建

项目 README 规定使用 Docker 构建，不使用 WSL 或本机 ESP-IDF。构建脚本位于该项目
上一级仓库的 `scripts` 目录，默认 Docker 镜像为：

```text
espressif/idf:v5.5.1
```

应在 Fightpad Bringup 仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\docker-build-esp32c6.ps1
```

需要干净构建时：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\docker-build-esp32c6.ps1 -Clean
```

脚本在容器内依次执行：

```text
idf.py set-target esp32c6
idf.py build
```

当前 `build/` 中已有以下主要产物：

| 文件 | 用途 |
| --- | --- |
| `fightpad12slim_c6_ble_hid.bin` | 应用程序二进制镜像 |
| `fightpad12slim_c6_ble_hid.elf` | 带符号的可执行文件，用于调试和符号分析 |
| `fightpad12slim_c6_ble_hid.map` | 链接映射和内存占用分析 |

现有 BIN 约 677 KiB。产物存在不代表当前源文件之后的修改已经重新编译；交付前应重新构建确认。

## 13. 烧录与启动

ESP32-C6 进入 UART Download Mode 的基本步骤：

1. 在复位期间将 GPIO9 保持为低电平。
2. 拉低后释放 `CHIP_PU`，完成复位。
3. 释放 GPIO9。
4. 通过 UART0 下载固件。

已正确配置 ESP-IDF 环境时可使用：

```powershell
idf.py -p COMx flash monitor
```

注意：工程构建策略是 Docker-only，但 README 的烧录示例使用本机 `idf.py`。实际团队流程中
应确认烧录是在容器内执行、使用本机 ESP-IDF，还是使用独立的 `esptool.py`。

## 14. 日志和调试观察点

主要日志 Tag：

```text
FP12_C6_HID
```

正常启动时应依次看到类似信息：

```text
app_main: start
app_main: nvs ready
app_main: event loop ready
app_main: nimble controller ready
app_main: pair button gpio ready ...
UART input ready on U0 GPIO16/GPIO17 at 115200 baud
app_main: uart ready
app_main: ble config ready for FP12Slim-C6
app_main: hid init requested
app_main: nimble host enabled
```

联调时建议重点观察：

- `UART input frame received`：首次收到有效 `FP` 帧。
- `transport mode: bluetooth/usb`：传输模式是否正确切换。
- `pair button pressed`：是否打开配对窗口。
- `advertising as FP12Slim-C6`：是否真正开始广播。
- `BLE connect ok`：BLE GAP 是否连接成功。
- `HID connected`：HID 层是否连接成功。
- `BLE subscribe`：主机是否订阅 HID Notification。
- `UART input timeout; neutral report restored`：输入是否超过 250 ms 中断。

部分数据日志使用 `ESP_LOGD`，当前日志级别若不是 Debug，默认不会显示。

## 15. README 与当前源码的差异

README 主要描述最初的 BLE HID Report 帧，但当前源码已经扩展。接手时以源码为准：

| 项目 | README | 当前源码 |
| --- | --- | --- |
| UART 帧类型 | 只描述 `FP` | 支持 `FP`、`FT`、`FB` |
| 广播时机 | 容易理解为上电即广播 | 必须 BLE 模式开启且配对窗口有效 |
| 配对控制 | 只说明 Just Works | GPIO13 开启 60 秒窗口并清绑定 |
| 电量 | 未描述 | 支持 BLE Battery Service 更新 |
| 启动握手 | 未描述 | TX 重复发送 `C6_DONE\n` |
| 模式切换 | 未描述 | UART 可关闭广播并主动断连 |

## 16. 当前风险和待确认项

以下不是已经确认的故障，而是接手后应优先验证的设计边界：

1. **断线重连**：连接成功会关闭配对窗口，断开后重新广播会被窗口条件阻止。
2. **绑定持久化**：代码开启 bonding，但完整 `sdkconfig` 未启用 NimBLE NVS Persist。
3. **UART0 复用**：Console 日志、下载通道、`C6_DONE` 和控制协议共享 UART0。
4. **配对按键双沿触发**：稳定按下和稳定释放都会进入 `trigger_pairing_mode()`。
5. **上电按键状态**：GPIO13 的启动电平被当作空闲电平，上电按住按键可能导致极性判断错误。
6. **电量范围**：`FB` 的电量值未限制为 0～100。
7. **USB 供电标志**：已经接收并保存，但还没有实际消费者。
8. **协议扩展能力**：帧无长度和版本字段，只能通过新增固定 Type 扩展。
9. **绑定清除策略**：每次打开新的配对窗口都会清除所有已保存的 Peer。
10. **工程模块化**：全部逻辑在一个约 1000 行的 `main.c` 中，后续扩展时维护成本会增加。

## 17. 建议的接手阅读顺序

第一次阅读建议按以下顺序，不需要从文件第一行一直读到底：

1. `app_main()`：先理解启动顺序和系统组成。
2. 顶部宏和全局状态：掌握引脚、周期、协议常量和状态机变量。
3. `parse_uart_byte()` 与 `handle_uart_frame()`：理解输入协议。
4. `compose_hid_report()` 与 `report_task()`：理解手柄报告输出。
5. `set_transport_bt_enabled()`：理解 USB/Bluetooth 模式边界。
6. `trigger_pairing_mode()`、`pair_button_task()`、`start_advertising()`：理解配对条件。
7. `gap_event()` 和 `hidd_event_callback()`：理解两套 BLE 事件来源。
8. `configure_security()`：最后确认安全和绑定策略。

## 18. 建议的最小实机验证清单

1. 上电后确认 UART TX 在约 2 秒内重复输出 `C6_DONE`。
2. 发送合法 `FT` Bluetooth Enable 帧，确认日志切换为 Bluetooth。
3. 按下 GPIO13，确认开始 60 秒 BLE 广播。
4. 手机或电脑扫描并连接 `FP12Slim-C6`。
5. 发送单按钮 `FP` 帧，确认 HID Button 映射正确。
6. 分别验证上下左右、四个对角和相反方向抵消。
7. 验证 X/Y 的 -127、0、127 三个典型值。
8. 停止发送 `FP` 超过 250 ms，确认所有输入自动释放。
9. 发送 `FB` 的 0%、50%、100%，确认主机端电量更新。
10. 发送 USB Mode `FT` 帧，确认报告中立、停止广播并断开连接。
11. 重新切回 Bluetooth，验证是否需要再次按配对键才能广播。
12. 断电重启后验证已绑定设备能否自动重连。

## 19. 后续重构建议

在功能继续增加时，可以考虑把 `main.c` 拆成以下边界：

```text
main/
├── main.c                  只保留系统装配和 app_main
├── uart_protocol.c/.h      帧解析、校验和类型定义
├── gamepad_report.c/.h     D-pad 与 HID 报告转换
├── ble_hid.c/.h            HID Device、GAP 事件和报告发送
├── pairing.c/.h            配对键、窗口和绑定策略
└── transport_state.c/.h    USB/Bluetooth 模式状态机
```

在完成实机行为确认之前，不建议仅为拆文件而立即重构。当前最有价值的第一步是先把
断线重连、绑定持久化和 UART0 混合流三个行为验证清楚，再确定状态机边界。

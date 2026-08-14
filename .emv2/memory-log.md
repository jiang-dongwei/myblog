# 记忆日志

## 2026-07-13: S1 固件信息UART发送 — 已完成

- 步骤编号: S1 (S1-A + S1-B)
- 讨论ID: 20260712-fw-info-uart
- 功能: 读取ESP32固件信息(SDK/Plat/Board/CPU)，通过UART(GPIO16/GPIO17)发送给RP2350

### 实现摘要

**修改文件**:
- `main/main.c`: 新增 `#include <stdio.h>`, `#include "esp_idf_version.h"`; 新增 FW_INFO 帧协议宏 (0x49 'I'), CPU 架构映射宏; 新增 `fw_info_build_payload()` + `send_fw_info_frames()` 函数; `app_main()` 中 UART 初始化后发送固件信息
- `main/CMakeLists.txt`: 添加 `PROJECT_NAME` 编译宏

**协议设计**:
- 帧类型: `I` (0x49), 8字节帧格式, XOR校验
- Byte2: flag(bits7-6) + seq(bits5-0), flags: 11=首帧, 10=中间帧, 01=尾帧, 00=单帧
- Byte3-6: 4字节payload数据
- Payload格式: `SDK=5.5.1\nPlat=esp32c6\nBoard=fightpad12slim_c6_ble_hid\nCPU=RISC-V\n`
- 发送时机: UART初始化后、C6_DONE之前立即同步发送

**字段来源**:
- SDK: `esp_get_idf_version()` 运行时API, 去掉 'v' 前缀
- Plat: `CONFIG_IDF_TARGET` 编译宏
- Board: `FW_INFO_BOARD_NAME` 宏 → `"Fightpad12Slim_C6_BLE_HID"`（规范化大小写）
- CPU: `CONFIG_IDF_TARGET_ARCH_RISCV` → "RISC-V"

---

## 2026-07-15: S2 降低功耗 — 已规划

- 步骤编号: S2 (S2-A + S2-B + S2-C + S2-D)
- 讨论ID: 20260715-power-saving
- 目标: 降低空闲/非连接状态功耗，不影响正常工作

### 子步骤

| 步骤 | 内容 | 优先级 |
|------|------|--------|
| S2-A | CPU 固定降频 160→80MHz (sdkconfig) | 1 |
| S2-B | BLE 广播间隔 30-50ms → 100-200ms | 2 |
| S2-C | Auto Light Sleep + FreeRTOS Tickless + UART 唤醒阈值 | 3 |
| S2-D | GPIO13 轮询改中断 + RTC GPIO 唤醒 | 4 |

### 关键决策
- 固定降频到 80MHz（不做动态调频）
- Light Sleep 只在空闲态生效，不影响 BLE 连接/广播
- UART 唤醒阈值设 3 字节（防噪声）
- GPIO13 从轮询改为下降沿中断，阻塞等待任务通知，零空闲唤醒

---

## 2026-07-17: S3 BLE 自动重连 — 已规划

- 步骤编号: S3 (S3-A + S3-B)
- 讨论ID: 20260717-ble-auto-reconnect
- 目标: 配对成功后，开关机手柄自动回连，无需按GPIO13

### 根因
`start_advertising()` 第894行检查 `pairing_window_active()`。连接成功后窗口被关闭，断连后广播无法重启。

### 子步骤

| 步骤 | 内容 | 优先级 |
|------|------|--------|
| S3-A | 广播与配对窗口解耦：移除start_advertising中的pairing_window条件，删除窗口过期停广播代码 | 1 |
| S3-B | 全时段广播保活：保活逻辑不限于配对窗口 | 2 |

### 实现摘要 (2026-07-17)

**修改文件**: `main/main.c` (3处改动)

1. `start_advertising()` (L894): 移除 `|| !pairing_window_active()` 条件 — 广播不再依赖配对窗口
2. `pair_button_task` (L463-469): 删除配对窗口过期停止广播的代码块
3. `pair_button_task` (L455): 广播保活条件移除 `pairing_window_active()` 限制 → 全时段保活

**编译**: ✅ 通过，0 错误 0 警告

### 关键决策
- 改动3处，均在 main/main.c
- pairing_window 只控制清 bond / 允许新设备配对
- 广播不再依赖配对窗口
- 安全配置不变（bonding + SC 已开启）

---

## 2026-07-20: S3-C BLE 重连修复 — 已完成

- 步骤编号: S3-C
- 讨论ID: (无，在计划模式下完成)
- 功能: 修复电脑蓝牙开关后无法自动重连

### 实现摘要

**修改文件**: `main/main.c` (4处改动)

1. 新增 `RECONNECT_ADV_DELAY_MS 1500` + `s_disconnect_tick` 变量
2. `BLE_GAP_EVENT_DISCONNECT` 移除 `start_advertising()` —— gap_event 运行在 NimBLE Host 任务上下文，直接调用 ble_gap_adv_start() 不可靠（NimBLE 文档警告）
3. `pair_button_task` 保活增加 1.5s 延迟检查 —— 产生"消失间隙"重置主机自动连接状态机
4. `BLE_GAP_EVENT_ENC_CHANGE` 增加加密失败处理 —— 清理 bond + 断开 → 下次走 REPEAT_PAIRING

### 关键决策
- 广播重启从 gap_event(context=NimBLE Host) 移到 pair_button_task(context=FreeRTOS task)
- 1.5s 延迟 = 给电脑蓝牙栈稳定时间 + 产生广播间隙

---

## 2026-07-20: S4 定向广播 + 断连空闲降速 — 已规划

- 步骤编号: S4 (S4-A ~ S4-D)
- 讨论ID: 20260720-directed-adv-sleep
- 功能: 断连+按键→定向广播快速重连；断连1min无重连→200ms慢广播省电

### 子步骤

| 步骤 | 内容 | 优先级 |
|------|------|--------|
| S4-A | 保存配对主机地址 + 定向广播基础结构 | 1 |
| S4-B | 断连1分钟降速到200ms慢广播 | 2 |
| S4-C | UART按键活动触发定向广播burst | 3 |
| S4-D | 状态机保护 + 边界条件 + 编译验证 | 4 |

### 关键决策
- 定向广播用 BLE_GAP_CONN_MODE_DIR + high_duty_cycle=1 + 1.28s burst
- 慢广播固定 200ms（用户确认）
- 1分钟计时从断连时刻开始（复用 s_disconnect_tick）
- 每次按键都重启 1.28s 定向 burst
- 首次配对前无 peer addr → 跳过定向，用普通广播

---

## 2026-08-13: S5 Xbox Layout BLE HID — 已确认开发

- 步骤编号：S5-A ~ S5-F
- 讨论ID：`20260813-xbox-layout-ble-hid`
- 分支：`Fix-Controller-type`
- 目标：Xbox Profile下由Windows/Steam/SDL按Xbox风格解释标准BLE HID，不实现
  Microsoft XInputHID、XUSB或GIP，不冒用微软身份。

### 已确认规则

- Profile v1保持既有 `FM` Mode / `FA` ACK 8字节协议，Xbox Profile ID为`1`。
- RP2350输入映射以USB `XInputDriver::process()`为唯一依据：B1~B4=A~Y、
  S1/S2=View/Menu、A1=Guide，L2/R2为`0x00/0xFF`数字扳机。
- A2、Turbo bit14、右摇杆和振动不导出。
- `Fightpad Xbox Layout`使用项目VID/PID `1209:2040`和Gamepad appearance。
- Profile变化时ACK完成后仅重启C6，启动后清旧bond并打开30秒配对；相同Profile
  不重启、不清bond。
- UART0 GPIO16/GPIO17专用于RP2350二进制协议；主Console切到USB Serial/JTAG。
- MAC/Profile bond隔离由`Fix-MAC-connection`分支处理；本分支不修改RP2350工程。

### 开发结果

- S5-A~S5-F 源码实现完成；三组宿主测试通过。
- ESP32-C6 构建通过，Xbox Profile 为独立标准 HOGP Game Pad 描述符，无振动输出报告。
- UART0 保持 GPIO16/GPIO17、115200 8N1，ESP-IDF Console 已切换到 USB Serial/JTAG。
- GPIO13 保持内部上拉、低电平有效；配对窗口为 30 秒，FS03 优先于 Connected。
- Windows 设备识别、旧 bond 清理后的重新配对、全部按键映射仍需实机验收。

---

## 2026-08-13: S5第一次实机验证失败并返工

- 用户确认目标是Web Gamepad网站识别Xbox/`mapping: standard`，不是更改蓝牙名称。
- 实测Xbox模式不可发现；根因是长名称令Legacy Advertising超过31字节。
- 所有Profile设备名恢复`FP12Slim-C6`；Generic/Keyboard/PS保留项目VID/PID。
- 仅Xbox Profile使用Chromium Windows已登记的Xbox One S Bluetooth PnP身份
  `045E:0B20`，并按`MapperXboxBluetooth`重排Button 1..15与Axis 0..5/Hat。
- 状态格式升级到v4，首次启动会清除旧bond和旧HID缓存并打开30秒配对。
- 仍无振动Output Report；静态测试和ESP32-C6构建通过，等待第二次实机验证。

---

## 2026-08-13: S5第二次实机验证失败，移植Xbox One S 1708描述符

- 第二次实测仍显示`Unknown Gamepad (0000 0000)`，说明仅采用Chromium原始映射用
  `045E:0B20`和9字节自定义报告不足以让Windows选择Xbox兼容设备路径。
- 参考`ESP32-BLE-CompositeHID`的Xbox One S Model 1708实现，仅重做Xbox Profile：
  PnP改为`045E:02FD/0408`，报告描述符改为字节一致的334字节版本。
- Xbox主输入Report 1改为16字节：四个16位摇杆轴、两个10位扳机、Hat、15按钮和
  Share字节；RP2350 UART没有右摇杆，所以右摇杆固定`0x8000`。
- 保留描述符中的Report 2、8字节Output Report 3和Report 4用于枚举兼容；固件不执行
  Output Report，因此仍不支持振动。
- 持久化格式升级到v5，v1~v4首次启动迁移时清旧bond并进入30秒配对，防止Windows
  沿用旧描述符缓存。
- ESP32-C6完整编译通过，生成`build/fightpad12slim_c6_ble_hid.bin`；Xbox标准识别仍需
  删除Windows旧设备后重新配对并由实机确认。

---

## 2026-08-13: S5第三次实机回归——Xbox不可发现与默认Profile修复

- 实测PS5可以连接，但Xbox按GPIO13后PC完全搜不到。
- 确认根因1：Xbox 1708描述符解析为4个Report特征，而`sdkconfig`仍保留NimBLE默认
  `CONFIG_BT_NIMBLE_SVC_HID_MAX_RPTS=3`，HID GATT数据库无法容纳全部Report。
- 确认根因2：状态格式已升级v5，但`ble_profile_store_load()`迁移入口仍只接受v1~v3，
  v4状态会被判无效并回退；Profile写Flash本身发生在Mode ACK/重启前，不依赖BLE连接。
- 将NimBLE Report上限改为4并写入`sdkconfig.defaults`，同时加入编译期下限检查。
- 状态格式升级v6；全新/无效NVS和v1~v5旧状态首次启动都强制Xbox、清bond并打开一次
  30秒配对，pending随后清除，后续普通开机仍保持原绑定设备的安全重连逻辑。
- 四组宿主测试通过，ESP32-C6完整构建通过，BIN为
  `build/fightpad12slim_c6_ble_hid.bin`；等待实机确认Xbox广播和配对。

---

## 2026-08-13: S5第四次实机回归——bond与双模式持久化修复

- 实测证明“首次/迁移强制Xbox并清bond”破坏了正常开机重连；C6默认状态和v1~v5迁移
  现均不再附加pending，迁移保留用户原Profile。
- 实测重启回PS5的跨芯片根因位于RP2350：菜单写入BLE Profile时遗漏protobuf
  `has_bluetoothProfile`。已补齐有效位，并在旧配置初始化时把BLE默认值设为Xbox(1)。
- USB控制器类型仍由`GamepadOptions.inputMode`独立持久化；BLE控制器类型由
  `FightpadESP32ProxyOptions.bluetoothProfile`独立持久化。
- C6运行时Profile真正变化会先保存NVS和ACK，再自行重启一次；相同Profile不重启。
- Xbox One S描述符和PnP身份仍只能提供Xbox风格BLE HID布局，不能保证Windows选择真正
  XInput/WGI设备路径，因此浏览器`Unknown`不能继续作为纯ESP-IDF HOGP代码的可承诺结果。
- C6三组主机测试与ESP-IDF编译通过；RP2350按用户约定未编译，只完成静态差异检查。

---

## 2026-08-13: S6改用Xbox Series X|S 1914原生BLE Profile

- 澄清芯片边界：ESP32-C6不支持Classic Bluetooth/BR/EDR，因此不再尝试旧Xbox经典
  蓝牙路径；本轮采用Xbox Series X|S 1914原生BLE HOGP Profile。
- Xbox PnP从One S 1708的`045E:02FD/0408`改为1914的`045E:0B13/0509`，并使用
  参考实现序列号`3039373130303637313034303231`。
- HID Report Map从334字节1708版本改为逐字节一致的283字节1914版本，只保留16字节
  Input Report 1与8字节Output Report 3；固件继续忽略输出，不支持振动。
- 现有16字节Xbox输入编码与1914 Report 1兼容，按键、方向、摇杆和数字扳机语义不变。
- 保留普通开机不清bond、GPIO13显式配对、Repeat Pairing按peer清旧键、定向/白名单
  重连、UART0和Profile持久化逻辑；没有修改RP2350工程。
- 参考描述符逐字节对比通过（283/283）；四组主机测试通过；ESP32-C6 Ninja构建通过。
- BIN：`build/fightpad12slim_c6_ble_hid.bin`，712288字节，SHA256
  `CFE79A02FDDB8ED0CA209DB8C48909733209266920CAB6FB7B040107865FB11F`。
- Windows是否选择`Bluetooth LE XINPUT`设备路径及浏览器是否显示Xbox/standard，必须
  删除旧设备缓存并重新配对后由实机确认，静态编译不能代替该结果。

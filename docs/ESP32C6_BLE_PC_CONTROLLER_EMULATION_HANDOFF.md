# ESP32-C6 BLE PC 控制器模拟：AI 协作任务书

文档状态：可直接交给 ESP32-C6 工程 AI 执行  
需求来源：Fightpad12Slim 多类型 BLE Controller Profile 返工  
RP2350 工程：`E:\ComporyProject\aa\GP2040-CE`  
ESP32-C6 权威工程：`E:\WorkSpace\C_WorkSpacee\ESP-IDF5.2\.espressif\release-v5.2\esp32c6_ble_hid_gamepad_test`  
UART 接口依据：RP2350 工程中的 `docs/ESP32C6_BLE_PROFILE_HANDOFF.md`

> 当前 GPIO13 配对键无 `Pairing` 的回归问题，先按
> `docs/ESP32C6_GPIO13_PAIRING_REGRESSION_HANDOFF.md` 完成修复和验收，再继续
> 本文的控制器识别实验。

## 0. 给执行 AI 的直接指令

请在上面的 ESP32-C6 权威工程中完成本任务。先审计，后修改；不要修改
`E:\ComporyProject\aa\GP2040-CE`，不要自行改变既有 UART Profile ID、帧格式
或 ACK 结果码。

本任务不是把 BLE 广播名称改成 Xbox/PS5，也不是只更换按键顺序。目标是：

1. ESP32-C6 作为 BLE Peripheral/HID Device，把 RP2350 UART 输入转换为 BLE
   HID 输入报告。
2. Windows PC 能完成配对、枚举、重连并持续接收全部按键。
3. 每个菜单 Profile 都必须明确说明主机实际把它识别成什么类型。
4. 对可行的目标做到协议/报告级兼容；对 ESP32-C6 硬件不可能支持的目标，
   必须给出源码和协议证据，不得用名称或 VID/PID 冒充完成。
5. 不要求连接真正的 Xbox、PlayStation 或 Nintendo 主机。

## 1. 产品目标与验收等级

目标主机优先级：

1. Windows 10/11 PC。
2. SuperStation One、MiSTer 等使用 Linux/SDL 的模拟游戏平台。
3. Android/macOS 仅作为附加兼容性，不作为首轮阻塞条件。

“识别成功”分为四级，执行 AI 必须在兼容矩阵中逐项标注：

| 等级 | 含义 | 是否合格 |
|---|---|---|
| L0 | 蓝牙列表只显示了目标名称 | 不合格 |
| L1 | 配对成功并枚举为标准 HID Gamepad/Keyboard | Generic/Keyboard 最低要求 |
| L2 | Windows GameInput、SDL/Steam 能得到正确按钮、方向和轴 | 所有保留 Profile 的最低要求 |
| L3 | 主机 API 按 Xbox/PlayStation 等目标家族识别 | 对应命名 Profile 的目标 |
| L4 | 可连接并通过原厂游戏主机认证 | 本项目明确不要求 |

测试网站显示一个自定义名称不能证明 L3；修改 VID/PID 也不能单独证明 L3。

## 2. 芯片职责边界

### RP2350 已完成且本任务不得改动

- 保存独立的 `bluetoothProfile`。
- 菜单 Profile：Generic=0、Xbox=1、Keyboard=2、PS5-PC=3。
- 通过 8 字节 XOR 帧发送 `FM` Mode 命令。
- 解析 C6 返回的 `FA` ACK，并显示 Applying/Pair Again/Error。
- 持续发送现有 `FP` 按键帧、`FT` Transport 帧和 `FB` Battery 帧。

### ESP32-C6 是本任务主要修改对象

- BLE GAP/GATT/HOGP 服务和安全配置。
- 广播身份、设备名称、Appearance、PnP ID/VID/PID 策略。
- HID Report Map、Input/Output/Feature Report 定义。
- 归一化 Fightpad 状态到各 Profile 报告的编码。
- Profile 切换、NVS、绑定、GATT 缓存、重新配对和重连。
- 主机输出报告处理；无硬件支持的震动/灯光可以安全忽略，但不能破坏连接。

## 3. 当前代码事实

先阅读并以当前源码为准：

- `main/main.c`：NimBLE/HID 生命周期、广播、连接、安全、GPIO13 配对、UART。
- `main/ble_profiles.c/.h`：四套 Report Map、设备身份和报告编码器。
- `main/ble_profile_state.c/.h`：Profile 变化决策和 pending flags。
- `main/ble_profile_store.c/.h`：NVS Profile 状态。
- `main/uart_protocol.c/.h`：`FM`/`FA` v1 协议。

当前实现仍只是兼容性实验：

- Xbox 和 PS5-PC 使用标准 HID usages。
- 所有 Profile 使用同一 Fightpad VID/PID 策略。
- PS5-PC Report Map 没有 DualSense 专有 Input/Output/Feature Reports。
- Profile 改变后会设置 `CLEAR_BONDS | PAIRING`，重启后清 C6 本地 bond，
  再开启 30 秒配对。
- 当前实现可能在相同 BLE Identity Address 下改变 GATT/HID 数据库；Windows
  仍保存旧 bond/GATT 缓存时，会发生 `Pair Again` 后无法连接或报告不更新。

不要把现状描述成已经完成 Xbox/PS5 模拟。

## 4. 硬件与协议硬限制

ESP32-C6 只支持 Bluetooth LE，不支持 Bluetooth Classic BR/EDR。因此：

- 标准 BLE HID/HOGP、BLE Keyboard、采用 BLE 传输的控制器兼容实验可做。
- 原生无线协议依赖 Bluetooth Classic 的 DS4、DualSense、Switch Pro 等目标，
  不能仅靠 ESP32-C6/NimBLE 完整复现。
- 不能把 USB GP2040-CE 的描述符直接复制到 BLE；USB HID 与 BLE HOGP 的
  服务发现、Report Reference、安全、绑定和缓存流程不同。

执行 AI 必须先产出 `docs/BLE_PROFILE_CAPABILITY_MATRIX.md`，对每个候选目标
写明传输层、协议来源、开源许可证、ESP32-C6 可行性和预计识别等级。

参考资料：

- ESP32-C6 Bluetooth LE only：
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-guides/ble/overview.html
- Bluetooth SIG HID over GATT：
  https://www.bluetooth.com/specifications/specs/hid-over-gatt-profile-hogp/
- Windows GameInput HID 映射与测试：
  https://learn.microsoft.com/en-us/xbox/gdk/docs/features/common/input/hardware/input-hardware-mapping
- Bluepad32 是 Controller Host，不是本任务需要的 Peripheral 模拟库：
  https://github.com/ricardoquesada/bluepad32
- ESP32-BLE-Gamepad 可参考标准 HOGP 设备结构，但不得整体迁移 Arduino
  依赖；当前工程继续使用 ESP-IDF 5.2/NimBLE/esp_hid：
  https://github.com/lemmingDev/ESP32-BLE-Gamepad

## 5. Profile 产品定义

### 5.1 Generic BLE

必须达到 L2，作为量产保底模式：

- 标准 HOGP Game Pad，Generic Desktop/Game Pad usage。
- Windows、SDL、Steam、浏览器 Gamepad API 能接收所有 Fightpad 输入。
- 普通重启保留 bond 并自动重连。
- 不伪装任何第三方品牌。

### 5.2 Keyboard BLE

必须达到 L2：

- 标准 HOGP Keyboard。
- 支持同时按键、正确释放和断连/超时中和，不能粘键。
- 普通重启保留 bond 并自动重连。

### 5.3 Xbox BLE PC

这是实验性协议兼容目标，不是更名目标：

- 先确认可合法引用的 BLE Xbox PC 报告/GATT 资料和许可证。
- 验收必须说明 Windows 中是 Generic HID、GameInput Gamepad 还是实际 Xbox
  家族/XInput 设备。
- 如果只能达到 L2，菜单/日志必须明确为 `Xbox Layout` 或
  `Xbox-compatible HID`，不能宣称 Xbox 仿真完成。
- 不要求连接 Xbox 主机，不实现主机认证。

### 5.4 PS5-PC

先做能力审计，再决定保留名称：

- 当前 ESP32-C6 不能凭标准 BLE HOGP Report Map 成为真正 DualSense。
- 如果目标协议依赖 Classic BR/EDR，必须标记为硬件不可实现。
- 若最终只能提供 PlayStation 按钮顺序的标准 HID，应改称
  `PlayStation Layout`，验收等级按 L2，不得称为 DualSense/PS5 模拟。
- 不要求连接 PS5 主机，不实现认证、自适应扳机、音频或完整触觉协议。

不要在本轮新增 Switch/PS3/PS4 Profile ID。若能力审计证明需要新增，先提交
协议扩展提案，不得单方面改变 UART v1。

## 6. 配对和多 Profile 身份设计

当前“C6 清本地 bond，但 PC 保留旧 bond”不是可接受的量产体验。执行 AI
需要评估以下方案并选择一种，有实机证据后记录决策：

### 推荐方案：每 Profile 使用稳定且不同的 BLE Identity

- 每台设备、每个 Profile 都有唯一且跨重启稳定的 identity/address。
- 地址可由芯片唯一 MAC 加 Profile ID 安全派生后保存到 NVS，但必须遵守
  Random Static Address 位规则并确认 ESP-IDF/NimBLE API 支持。
- 每个 Profile 维护自己的最近绑定主机或 bond 记录。
- A -> B -> A 切换时，PC 中两个身份互不覆盖；返回 A 可恢复 A 的旧绑定。

### 备选方案

- 单一不可变的复合 HOGP 描述符：切换时只改变活跃 Report ID。优点是 bond
  稳定；缺点是主机始终看到同一个复合 Fightpad，通常不能达到目标家族 L3。
- 同一地址更换 GATT/HID 数据库：只有在 Service Changed、GATT cache 和双端
  bond 生命周期被完整验证后才能采用。单方面 `ble_store_clear()` 不合格。

Profile 未变化的普通开机必须只重连，不得再次配对。只有以下情况允许进入
30 秒配对窗口：

- GPIO13 显式配对按键。
- 切换到一个从未绑定过的独立 Profile Identity。
- 用户明确执行恢复出厂/清绑定操作。

## 7. 分阶段执行任务

### A. 基线与证据

1. 不改代码，记录当前四个 Profile 的广播、GATT 服务、Report Map、VID/PID、
   BLE address、bond 数量和 Windows 枚举结果。
2. 复现 `Pair Again` 后连接失败，保存 C6 GAP/SMP 日志及 Windows 端现象。
3. 证明失败发生在连接、加密、服务发现、订阅还是输入报告阶段。

### B. Generic/Keyboard 稳定化

1. 修复配对、重连、订阅和 neutral report。
2. 完成全部 Fightpad 按键映射测试。
3. 保证同 Profile 重启不清 bond、不打开普通配对窗口。

### C. Profile Identity 隔离

1. 实现并测试选定的多身份/bond 方案。
2. A -> B -> A 循环至少 10 次，不能要求用户每次从 Windows 删除设备。
3. Profile 切换重启期间继续正确返回 `FA` ACK 和 `FS` 状态。

### D. Xbox BLE PC 兼容实验

1. 仅使用有明确来源和许可证的描述符/协议资料。
2. 完整实现要求的 Input/Output/Feature Reports；未知主机请求要记录并安全响应。
3. 用 Windows GameInput/SDL/Steam 验证主机分类和全部输入。
4. 达不到 L3 时保留为 Xbox Layout，不伪报完成。

### E. PS5-PC 能力结论

1. 证明目标传输是否能在 C6 BLE-only 硬件上实现。
2. 可行则提交协议来源、实现范围和测试结果。
3. 不可行则把 Profile 降级为 PlayStation Layout，并保留标准 HID L2 兼容。

## 8. 必须保留的现有行为

- UART `FP/FT/FB/FI/FS/FM/FA` 固定 8 字节 XOR 帧。
- Profile v1 编号和 sequence/ACK 语义。
- GP33/GP34 的 USB/BT 传输和 C6 使能逻辑。
- GPIO13 的 30 秒显式配对窗口。
- RP2350 菜单输入锁时，C6 输出 neutral HID。
- 普通开机只重连最近绑定主机。
- 未显式配对时禁止陌生主机抢连。

## 9. Windows 与模拟平台验收

每个保留 Profile 都要填写以下结果，不能只写“连接成功”：

| 项目 | 记录内容 |
|---|---|
| Bluetooth | 显示名、identity address、首次配对、重启重连 |
| Device Manager | Hardware IDs、服务、驱动 |
| HID | Report Map、Report ID、输入长度、输出/特征报告 |
| joy.cpl/GameInput | 主机分类、按钮、Hat、轴、扳机 |
| SDL/Steam | 控制器名称、GUID/类型、完整映射 |
| Browser tester | 每个物理按键是否变化，释放是否归零 |
| Linux/MiSTer 类 | 是否枚举、能否映射、断线重连 |
| Profile switch | A->B->A 是否需要删除系统设备，是否发生 Pair Again 失败 |

最低实机用例：

1. 新设备首次配对。
2. 断电 10 秒后自动重连。
3. PC 蓝牙关闭再打开后重连。
4. Profile 不变时重启 20 次，不得要求重新配对。
5. Profile A/B 循环切换 10 次。
6. 同时按下至少 6 个按键，再全部释放，无粘键。
7. 连接中 UART 输入停止超过现有 stale timeout，主机收到 neutral。
8. 显式 GPIO13 配对新 PC 后，旧 PC 不得在非配对窗口抢连。

## 10. 交付物

执行 AI 完成后必须提供：

- 源码补丁和修改文件列表。
- `docs/BLE_PROFILE_CAPABILITY_MATRIX.md`。
- `docs/BLE_PROFILE_WINDOWS_TEST_RESULTS.md`。
- 一段串口日志，覆盖 Profile 切换、ACK、重启、配对、加密、订阅和输入。
- ESP32-C6 构建命令、BIN 路径、大小和 SHA-256。
- 未完成项及其原因；不得把“名称正确”写成“协议模拟完成”。
- 如果需要 RP2350 协议变化，单独写提案，不直接修改 RP2350 工程。

## 11. 执行 AI 首条回复模板

请执行 AI 读完代码后先回复以下内容，再开始修改：

```text
已确认当前工程路径：...
当前 ESP-IDF/芯片/蓝牙栈：...
当前四个 Profile 的真实识别等级：...
Pair Again 失败发生阶段：尚待测试 / 已定位为...
ESP32-C6 BLE-only 对各目标的能力矩阵：...
计划首先完成：Generic/Keyboard 基线 -> 多身份配对 -> Xbox PC 实验 -> PS5结论
预计修改文件：...
不会修改 RP2350 UART v1 协议。
```

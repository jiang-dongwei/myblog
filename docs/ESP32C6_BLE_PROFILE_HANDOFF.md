# RP2350 / ESP32-C6 BLE Profile 协作接口

状态：协议 v1，已确认，可进入实现  
适用 RP2350 工程：`E:\ComporyProject\aa\GP2040-CE`  
适用 ESP32-C6 工程：`E:\WorkSpace\C_WorkSpacee\ESP-IDF5.2\.espressif\release-v5.2\esp32c6_ble_hid_gamepad_test`

> 本文继续作为 RP2350/C6 UART Profile 协议依据。关于“让 PC 按目标手柄
> 类型识别”的产品目标、能力边界和验收方法，以
> `docs/ESP32C6_BLE_PC_CONTROLLER_EMULATION_HANDOFF.md` 为准；仅修改广播
> 名称、按钮顺序或 VID/PID 不视为完成控制器模拟。

> GPIO13 配对键按下后无 `Pairing` 的当前回归，以
> `docs/ESP32C6_GPIO13_PAIRING_REGRESSION_HANDOFF.md` 为修复和验收依据。

> 两边 AI 必须以本文为唯一接口依据。任何字节布局、Profile ID 或结果码
> 的变化，都必须更新本文并同时修改两边工程。

## 1. 芯片职责

### RP2350

- 提供 `Bluetooth Type` 菜单和 OLED 状态显示。
- 将蓝牙模式保存到 GP2040-CE 配置。
- 作为产品级 Profile 真值来源。
- 向 C6 发送 Profile 命令并等待 ACK。
- 继续发送现有按键、Transport 和 Battery 帧。

### ESP32-C6

- 保存最近一次接受的 Profile，作为 RP2350 暂时不可达时的启动回退。
- 在 BLE HID 初始化前选择设备身份、报告描述符和报告编码器。
- 管理绑定、重连、30 秒配对窗口和 Profile 变化重启。
- 向 RP2350 回复 ACK、BLE Status 和现有固件信息。

USB `InputMode` 与 BLE Profile 是两套独立配置，任何一边都不得把它们
自动同步或相互覆盖。

## 2. Profile 编号

协议中只传固定编号，不传 GP2040-CE 的 `InputMode` 枚举值。

```c
typedef enum {
    FIGHTPAD_BLE_PROFILE_GENERIC  = 0,
    FIGHTPAD_BLE_PROFILE_XBOX     = 1,
    FIGHTPAD_BLE_PROFILE_KEYBOARD = 2,
    FIGHTPAD_BLE_PROFILE_PS5_PC   = 3,
} fightpad_ble_profile_t;
```

RP2350 新配置字段建议为：

```proto
optional uint32 bluetoothProfile = 12 [default = 1];
```

字段归属：`FightpadESP32ProxyOptions`。读取时仍必须进行 `0..3` 范围
校验；越界值按 Generic 处理，并显示/记录回退原因。

菜单顺序：

1. `XBOX BLE`
2. `GENERIC BLE`
3. `KEYBOARD BLE`
4. `PS5 BLE (PC)`

菜单顺序不等于协议编号，代码必须通过表项显式映射。

## 3. UART 公共格式

所有控制帧继续使用现有固定 8 字节格式：

```text
byte 0..6：有效字段
byte 7：byte 0..6 逐字节 XOR
```

现有类型保持不变：

| byte 1 | 方向 | 含义 |
|---|---|---|
| `'P'` | RP -> C6 | Input report |
| `'T'` | RP -> C6 | Transport |
| `'B'` | RP -> C6 | Battery |
| `'I'` | C6 -> RP | Firmware info |
| `'S'` | C6 -> RP | BLE status |

新增类型：

| byte 1 | 方向 | 含义 |
|---|---|---|
| `'M'` | RP -> C6 | BLE Profile mode |
| `'A'` | C6 -> RP | BLE Profile acknowledgement |

解析器必须将 `'M'` 和 `'A'` 加入允许的第二魔数集合，否则现有流式解析
状态机会在第二字节丢弃新帧。

## 4. Mode 命令

RP2350 到 ESP32-C6：

| 字节 | 字段 | v1 定义 |
|---:|---|---|
| 0 | magic | `0x46`，ASCII `F` |
| 1 | type | `0x4D`，ASCII `M` |
| 2 | version | `0x01` |
| 3 | profile | `0..3` |
| 4 | sequence | 每次新请求递增，8 位回绕 |
| 5 | flags | 见下表 |
| 6 | reserved | v1 必须为 `0` |
| 7 | checksum | byte 0..6 XOR |

Flags：

```c
#define BLE_PROFILE_FLAG_APPLY_NOW    0x01
#define BLE_PROFILE_FLAG_FORCE_REPAIR 0x02  // v1菜单暂不使用，保留
```

- USB Transport 下修改菜单：只保存 RP2350 配置，不发送 Mode，不重启 C6。
- 进入 Bluetooth Transport：发送 `APPLY_NOW`。
- Bluetooth Transport 中修改：保存后立即发送 `APPLY_NOW`。
- v1 未定义的 flag 位必须为 0；C6 应忽略未知位并记录日志。

## 5. ACK 帧

ESP32-C6 到 RP2350：

| 字节 | 字段 | v1 定义 |
|---:|---|---|
| 0 | magic | `0x46` |
| 1 | type | `0x41`，ASCII `A` |
| 2 | version | `0x01` |
| 3 | accepted profile | C6 最终接受的 `0..3` |
| 4 | sequence | 原样复制 Mode sequence |
| 5 | result | 见下表 |
| 6 | reserved | v1 必须为 `0` |
| 7 | checksum | byte 0..6 XOR |

Result：

```c
typedef enum {
    BLE_PROFILE_ACK_ACTIVE_UNCHANGED   = 0,
    BLE_PROFILE_ACK_RESTARTING         = 1,
    BLE_PROFILE_ACK_APPLYING_AT_BOOT   = 2,
    BLE_PROFILE_ACK_INVALID_FALLBACK   = 3,
    BLE_PROFILE_ACK_UNSUPPORTED_VERSION = 4,
    BLE_PROFILE_ACK_INTERNAL_ERROR     = 5,
} fightpad_ble_profile_ack_t;
```

- `ACTIVE_UNCHANGED`：模式已活动，不清绑定、不重启、不强制配对。
- `RESTARTING`：已持久化，C6 将独立重启并在启动后配对。
- `APPLYING_AT_BOOT`：HID 尚未初始化，本次启动直接应用，无需二次重启。
- `INVALID_FALLBACK`：Profile 越界，C6 接受 Generic 作为安全回退。
- `UNSUPPORTED_VERSION`：不改 NVS、不改当前模式；RP2350 显示协议错误。
- `INTERNAL_ERROR`：不应继续触发重启；RP2350 显示失败。

RP2350 只接受同时匹配 version 和 sequence 的 ACK。旧 ACK 可以记录但不能
结束当前请求。

## 6. 校验测试向量

两边必须在不启动 BLE 的情况下先验证这些固定帧。

### Xbox Mode，sequence `0x2A`，立即应用

```text
46 4D 01 01 2A 01 00 20
```

### 接受 Xbox 并准备重启

```text
46 41 01 01 2A 01 00 2C
```

### Xbox 已经活动，无需重启

```text
46 41 01 01 2A 00 00 2D
```

收到 checksum 错误的帧必须静默丢弃或仅输出限频日志，不得更新 NVS。

## 7. RP2350 状态机

建议由 `FightpadESP32ProxyAddon` 负责传输，不让菜单直接操作 UART：

```text
IDLE
  -> REQUEST_PENDING
  -> WAIT_ACK
      -> ACTIVE
      -> C6_RESTARTING
      -> PROTOCOL_ERROR
      -> TIMEOUT
```

规则：

- 新请求分配新 sequence。
- 立即发送，随后每 250 ms 重试。
- 建议 2 秒后判定超时；超时停止高频发送，避免 UART 永久刷屏。
- Transport 再次切换到 BT 或 C6 固件信息重新出现时，可以重新发起同步。
- 收到 `RESTARTING` 后显示 `APPLYING`，随后依靠现有 `'S'` 状态帧显示
  `PAIRING`。
- 屏幕配对提示应居中显示，不周期性加载旧画面，避免此前的闪烁问题。

菜单选中相同 Profile 时不创建新请求。

## 8. ESP32-C6 启动与重启状态机

### 冷启动或从 USB 休眠唤醒

1. 初始化 NVS 和输入 UART。
2. 读取 `ble_profile`，不存在或越界则使用 Generic/Xbox 产品默认迁移策略。
3. 在 `esp_hidd_dev_init()` 前提供约 500 ms 的 Profile 同步窗口。
4. 若窗口内收到有效 Mode：
   - 与 NVS 相同：回复 `ACTIVE_UNCHANGED` 或 `APPLYING_AT_BOOT`。
   - 与 NVS 不同：保存新值并标记 `clear_bonds_pending`、
     `pair_after_boot`，回复 `APPLYING_AT_BOOT`。
5. 超时未收到 Mode：使用 NVS 回退继续启动，不能永久等待 RP2350。
6. 根据最终 Profile 选择 descriptor/config/encoder，再调用
   `esp_hidd_dev_init()`。
7. 若存在 `clear_bonds_pending`，在开始广告前清理旧绑定，成功后清除标记。
8. 若存在 `pair_after_boot`，BLE host ready 后开启 30 秒配对，随后清除标记。

### BLE 已运行时收到新 Profile

UART 接收任务不得直接调用复杂 BLE 生命周期 API。应投递控制事件，由 BLE
控制任务执行：

1. 校验并保存新 Profile。
2. 原子保存 `clear_bonds_pending=true` 和 `pair_after_boot=true`。
3. 回复 `RESTARTING`，确认 UART 写入完成。
4. 延迟约 50 ms，执行 `esp_restart()`。
5. 新启动按上述启动流程清绑定并进入 30 秒配对。

如果断电发生在 ACK 与重启之间，持久化的 pending 标志保证下一次启动仍会
完成清理与配对。

## 9. Profile 模块结构

建议新增：

```text
main/ble_profiles.h
main/ble_profiles.c
main/uart_protocol.h
main/uart_protocol.c
```

`ble_profiles` 至少提供：

```c
const ble_profile_definition_t *ble_profile_get(uint8_t profile);
bool ble_profile_encode_report(
    uint8_t profile,
    const normalized_fightpad_state_t *state,
    uint8_t *out,
    size_t out_capacity,
    size_t *out_length);
```

Profile definition 包含：

- device name
- manufacturer/serial policy
- VID/PID/version policy
- HID report map 与长度
- 输入 Report ID 与长度
- report encoder

当前 `main.c` 的 BLE 连接、配对和 GPIO13 行为不应复制进每个 Profile。

## 10. 输入映射

RP2350 继续发送现有归一化 15-bit 按键、dpad、LX 和 LY。Profile 差异由
C6 encoder 负责，避免为四个模式创建四种 RP 输入帧。

物理 Turbo 位继续作为产品内部功能，不导出为主机额外按键。

### Xbox BLE

| Fightpad | Host |
|---|---|
| B1/B2/B3/B4 | A/B/X/Y |
| L1/R1 | LB/RB |
| L2/R2 | LT/RT，v1 为数字触发 |
| L3/R3 | Left/Right stick click |
| S1/S2 | View/Menu |
| A1/A2 | Guide/Share 或 Capture |

### PS5 BLE (PC)

| Fightpad | Host |
|---|---|
| B1/B2/B3/B4 | Cross/Circle/Square/Triangle |
| L1/L2/R1/R2 | 对应肩键/扳机 |
| L3/R3 | 对应摇杆按下 |
| S1/S2 | Create/Options |
| A1/A2 | PS/Touchpad click |

未实现的触摸坐标、陀螺仪、加速度计和自适应扳机字段保持中性。该模式只以
Windows/Steam/SDL 实验兼容为目标，不承诺 PS5 主机兼容。

### Keyboard BLE

使用 Fightpad12Slim 固定产品映射：

| Fightpad | Key |
|---|---|
| Up/Down/Right/Left | Arrow keys |
| B1/B2/R2/L2 | Left Shift/Z/X/V |
| B3/B4/R1/L1 | Left Ctrl/Left Alt/Space/C |
| S1/S2 | 5/1 |
| L3/R3 | `=`/`-` |
| A1/A2 | 9/F2 |

v1 不同步 Web Config 中 USB Keyboard Mapping。C6 必须根据完整按键状态产生
标准 keyboard press/release 报告，不能只在按下沿发送。

### Generic BLE

首先通过新模块复现当前 Generic gamepad 的方向、摇杆和普通按键行为，作为
重构回归基线。

## 11. BLE 身份策略

Profile ID 表示报告行为，不等价于某厂商授权身份。身份表必须支持至少两种
构建策略：

```text
Compatibility/Test：用于实验室验证主机识别
Production：使用明确获得授权或分配的 VID/PID 与 FIGHTPAD 品牌
```

不得把 Microsoft/Sony VID/PID 冒用方案默认为唯一量产路径。Xbox/PS5 参考
描述符若来自第三方开源项目，必须保留来源和许可证记录，并单独复核商标、
VID/PID 与主机认证要求。

## 12. 双工程任务边界

### 修改 GP2040-CE 的 AI

- 修改 `proto/config.proto` 并完成工程现有的 protobuf 生成流程。
- 修改滚轮菜单和显示状态。
- 在 ESP32 proxy 中实现 Mode 发送、ACK 解析、重试与状态。
- 保持现有 `'P'/'T'/'B'/'I'/'S'` 行为不退化。
- 不编译该工程，由用户执行构建和烧录。

### 修改 ESP32-C6 的 AI

- 先阅读当前 `main/main.c` 的 UART、HID、bond、pairing 和 GPIO13 流程。
- 添加 `'M'/'A'` 解析与测试向量。
- 添加 NVS Profile/pending 状态。
- 在 HID init 前选择 Profile。
- 按 Generic -> Keyboard -> Xbox -> PS5-PC 顺序提交功能。
- 不自行更改本文协议；发现冲突时先报告。

## 13. 联调验收表

| 场景 | 预期 |
|---|---|
| USB 下改变 BLE Profile | USB 类型不变，C6 不被唤醒/重启 |
| 切到 BT，Profile 未变 | 保留 bond，直接重连 |
| BT 下改变 Profile | ACK 后仅 C6 重启，进入 30 秒配对 |
| 同一 Profile 再次选择 | 不重启、不清 bond |
| C6 无 ACK | RP 2 秒超时并显示错误，不无限重启 |
| 错 checksum | 丢弃，不写 NVS |
| 错 version | 回复 unsupported，不改变当前 Profile |
| Profile 越界 | 回退 Generic 并明确报告 |
| C6 重启中断电 | 下次启动依据 pending 标志完成清 bond/配对 |
| 普通整机断电重启 | 不重新配对，只重连最近已绑定主机 |
| Generic | 当前基础映射无回归 |
| Keyboard | 按下、组合键、释放均无粘键 |
| Xbox | PC 测试平台映射符合表格 |
| PS5-PC | Windows/Steam/SDL 基础按键符合表格 |

## 14. 版本与交付

- 协议常量建议同时存在于两边各自的 `uart_protocol.h`，内容必须与本文一致。
- C6 固件信息帧应继续工作，并建议将协议版本加入日志或后续固件信息扩展。
- 量产记录必须把 RP2350 UF2 哈希与 ESP32-C6 BIN 哈希成对保存。
- 任一边发布协议不兼容版本时，必须升级 version，而不是复用 v1 字节含义。

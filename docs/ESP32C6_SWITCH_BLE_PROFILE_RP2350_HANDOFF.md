# ESP32-C6 Switch BLE Profile：RP2350 协作交接

状态：ESP32-C6 端已实现并构建通过，RP2350 端需要同步、审计和由用户构建验证  
日期：2026-08-17  
RP2350 工程：`E:\ComporyProject\aa\GP2040-CE`  
ESP32-C6 工程：`E:\WorkSpace\C_WorkSpacee\ESP-IDF5.2\.espressif\release-v5.2\esp32c6_fix_mac_connection`

## 1. 给 RP2350 端 AI 的直接任务

请只处理 RP2350/GP2040-CE 一侧：

1. 先执行 `git status --short`，保护当前所有未提交修改。
2. 审计 Profile 枚举、范围校验、菜单表、配置保存和 ESP32 proxy 同步路径。
3. 确保新增 Switch BLE Profile 的 UART 固定编号为 `4`。
4. 菜单增加 `SWITCH BLE`，选择后沿用现有保存成功再同步的流程。
5. 不改变 UART 协议版本、帧长度、校验算法和旧 Profile 编号。
6. 不修改 USB `InputMode` 和现有 USB Switch Pro 驱动。
7. 不运行 GP2040-CE 编译或构建；由用户自行编译烧录。

当前工作树中可能已经存在本交接所需的两处最小修改。不要盲目重复或覆盖，
应先对照本文逐项审计，再补充遗漏。

## 2. 功能目标

RP2350 的 Bluetooth Type 菜单新增一个独立 Switch 项：

```text
SWITCH BLE -> UART Profile 4 -> ESP32-C6重启 -> 广播 FP12-SW
```

ESP32-C6 Profile 4 已具备：

| 字段 | 值 |
|---|---|
| Profile | `4` |
| 广播名称 | `FP12-SW` |
| VID | `0x057E` |
| PID | `0x2009` |
| HID Input Report | ID `0x30`，payload 63字节 |
| 对外能力 | 4 axes、18 buttons、Hat |
| Bond | 独立 `fp_bond_sw` 工作区 |
| BLE身份 | 独立稳定 Static Random 地址 |

目标是让 Windows/Chrome 将其作为 Switch 风格标准手柄使用，不承诺连接真正
Nintendo Switch 主机，也不要求 RP2350 实现 Switch 握手、震动或 IMU 协议。

## 3. Profile 编号兼容性

必须保持原编号不变，只在末尾追加：

```cpp
enum class FightpadBluetoothProfile : uint8_t {
    Generic = 0,
    Xbox = 1,
    Keyboard = 2,
    PS5PC = 3,  // 当前实际对应PS4兼容BLE Profile
    Switch = 4,
};
```

合法范围应覆盖 `0..4`：

```cpp
static constexpr bool isValidFightpadBluetoothProfile(uint8_t value)
{
    return value <= static_cast<uint8_t>(FightpadBluetoothProfile::Switch);
}
```

标签函数增加：

```cpp
case FightpadBluetoothProfile::Switch: return "SWITCH BLE";
```

禁止重排旧编号。否则已保存的用户配置会被解释成其他模式，且 C6 Bond 工作区
会与错误的 BLE 身份对应。

## 4. UART 接口保持 v1

无需升级协议版本：

```cpp
FIGHTPAD_BLE_PROFILE_PROTOCOL_VERSION = 1
```

Mode 帧仍为固定8字节：

| Byte | Switch定义 |
|---:|---|
| 0 | `0x46`，`F` |
| 1 | `0x4D`，`M` |
| 2 | `0x01`，协议版本 |
| 3 | `0x04`，Switch Profile |
| 4 | sequence |
| 5 | flags，通常为 `APPLY_NOW=0x01` |
| 6 | `0x00` |
| 7 | Byte 0..6 XOR |

固定测试向量，sequence=`0x2B`：

```text
RP -> C6  Switch立即应用：46 4D 01 04 2B 01 00 24
C6 -> RP  接受并准备重启：46 41 01 04 2B 01 00 28
C6 -> RP  已是当前Profile：46 41 01 04 2B 00 00 29
```

ACK 解析、sequence匹配、250ms重试和2秒超时沿用现有实现，不为 Switch
单独创建另一套状态机。

## 5. RP2350 必查文件

### 5.1 Profile 公共定义

文件：`headers/addons/fightpad_ble_profile.h`

检查：

- 是否存在 `Switch = 4`。
- `isValidFightpadBluetoothProfile()` 是否接受4、拒绝5及以上。
- `normalizeFightpadBluetoothProfile(4)` 是否保持为Switch。
- 标签是否返回 `SWITCH BLE`。
- Profile 3仍保持现有PS4兼容语义，不改编号。

### 5.2 滚轮菜单

文件：`src/addons/scrollwheel_menu.cpp`

在 `kMenuBluetoothTypes[]` 中加入显式映射：

```cpp
{ "SWITCH BLE", SWMenuLevel::INFO,
  static_cast<uint8_t>(FightpadBluetoothProfile::Switch) },
```

菜单顺序可以放在PS4之后，但不得依赖数组下标作为协议编号，必须继续使用
`targetIndex`。

### 5.3 ESP32 Proxy

文件：

- `headers/addons/fightpad_esp32_proxy.h`
- `src/addons/fightpad_esp32_proxy.cpp`

现有实现通过公共合法性函数和数值字段发送 Profile，理论上无需 Switch 专属
分支。仍需审计：

- 是否存在硬编码 `<= PS5PC`、`< 4`、固定长度4数组或只处理0~3的switch。
- Profile 4是否能进入 Mode 帧 Byte 3。
- 收到 accepted profile 4的ACK时是否会正常结束请求。
- `RESTARTING` 后是否继续显示 Applying/Pairing，而不是判定未知模式。
- 配置保存失败时是否恢复旧值，不把仅存在RAM中的4发送给C6。

建议检查命令：

```powershell
rg -n "PS5PC|FightpadBluetoothProfile|bluetoothProfile|PROFILE.*3|< 4|<= 3" headers src
```

### 5.4 配置存储

`FightpadESP32ProxyOptions.bluetoothProfile` 当前是数值字段，Profile 4不要求修改
protobuf字段类型或编号。需要确认所有读取入口都经过
`normalizeFightpadBluetoothProfile()`，不能在其他位置再次限制为0..3。

### 5.5 协议文档

文件：`docs/ESP32C6_BLE_PROFILE_HANDOFF.md`

把旧文档中的以下内容同步扩展到Profile 4：

- Profile枚举和合法范围 `0..4`。
- 菜单列表增加 `SWITCH BLE`。
- Mode与ACK的Profile字段范围改为 `0..4`。
- 联调验收表增加Switch场景。

本文件是S11增量依据；旧主协议文档中仍写 `0..3` 的内容不能用于否定
Profile 4。

## 6. 必须保持不变

- UART帧长度仍为8字节。
- XOR校验仍覆盖Byte 0..6。
- UART协议版本仍为1。
- 输入状态帧 `P`、Transport帧 `T`、Battery帧 `B` 不变。
- RP2350继续发送归一化按键状态，由C6完成Switch报告编码。
- USB Controller Type与Bluetooth Type保持独立。
- 选择不同BLE Profile时，RP2350在强制保存后执行与USB Controller Type相同的
  500ms黑屏重启和启动Logo；C6仍按既有ACK流程自行应用并重启。
- 不清除Xbox、PS4或Switch Bond；Bond隔离由C6负责。

## 7. 静态验证

RP2350端AI完成修改后执行：

```powershell
rg -n "Switch = 4|SWITCH BLE|isValidFightpadBluetoothProfile" `
  headers/addons/fightpad_ble_profile.h `
  src/addons/scrollwheel_menu.cpp

git diff --check -- `
  headers/addons/fightpad_ble_profile.h `
  src/addons/scrollwheel_menu.cpp `
  src/addons/fightpad_esp32_proxy.cpp `
  docs/ESP32C6_BLE_PROFILE_HANDOFF.md
```

不要执行CMake、Ninja或任何GP2040-CE构建命令。向用户报告修改文件和静态检查
结果，构建烧录留给用户。

## 8. 双固件联调步骤

1. 用户自行构建并烧录RP2350固件。
2. 烧录C6固件：
   `build/fightpad12slim_c6_ble_hid.bin`。
3. 菜单选择 `BLUETOOTH TYPE -> SWITCH BLE`。
4. RP2350保存配置并发送Profile 4；C6回复`RESTARTING`并自行重启。
5. 首次进入Switch Profile时打开配对窗口，Windows扫描到`FP12-SW`。
6. Windows完成配对后打开Chrome Gamepad API测试。

目标结果：

```text
id: "Wireless Gamepad (STANDARD GAMEPAD Vendor: 057e Product: 2009)"
axes: 4
buttons: 18
mapping: "standard"
```

名称由Windows/Chrome映射决定，不应把名称文字作为唯一验收条件。核心验收是
`057E:2009`、4轴、18按钮、按键可用、断电重连和多Profile Bond隔离。

## 9. 回归场景

| 场景 | 预期 |
|---|---|
| Xbox -> Switch | C6重启；Switch首次配对或恢复Switch Bond |
| Switch -> PS4 -> Switch | 第二次进入Switch不重复配对，自动恢复 |
| Switch断电重启 | 当前Profile仍为Switch并自动重连 |
| USB模式修改为Switch BLE | 只保存配置，不改变USB控制器类型 |
| 再次选择当前Switch | 不清Bond、不重复重启 |
| Profile 5或更大 | RP侧回退Generic，不发送无效值 |
| C6无ACK | 现有超时流程结束，不无限刷UART |

## 10. 当前代码状态提醒

在生成本文时，RP2350工作树已经有多处用户未提交修改，且以下最小Switch改动
已经写入当前文件：

- `headers/addons/fightpad_ble_profile.h`：`Switch = 4`、合法范围和标签。
- `src/addons/scrollwheel_menu.cpp`：`SWITCH BLE`菜单项。

RP2350端AI的任务是审计、补齐遗漏、更新主协议文档并静态验证。禁止使用
`git reset --hard`、`git checkout --`或覆盖整个文件的方式处理这些改动。

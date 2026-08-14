# RP2350 蓝牙 Profile 持久化与启动同步回归修复任务书

## 1. 任务对象

请在以下权威 RP2350 工程中处理：

```text
E:\ComporyProject\aa\GP2040-CE
```

本任务书所在 ESP32-C6 工程仅用于提供协议和故障链背景。**不要修改本任务书所在的
ESP32-C6 工程**。不要恢复或覆盖 RP2350 工程中其他 AI 已完成的 OLED、BLE 状态、
UART、双模式显示和配置字段改动。

开始前必须：

1. 阅读本任务书。
2. 检查 `git status --short` 和相关文件的现有未提交差异。
3. 保留与本问题无关的所有用户改动。

## 2. 用户实机现象

1. 全新/第一次开机没有自动进入配对，属于正常。
2. PS 蓝牙配对成功后，在菜单切换 Xbox：C6 重启并要求重新配对。
3. Xbox 配对成功后关机重启，设备再次进入配对，不能自动重连。
4. 配对窗口超时后，OLED/运行状态又变回 PS。

## 3. 正确行为

### 3.1 Profile 发生变化

PS 与 Xbox 的 BLE PnP 身份、HID Report Map 和 GATT Report 集合不同，因此：

```text
PS -> Xbox
Xbox -> PS
```

真正切换 Profile 时，允许 C6 重启一次并要求用户重新配对一次。这不是本任务需要消除
的行为。

### 3.2 Profile 没有变化

配对成功后，如果 Profile 仍是 Xbox：

```text
普通关机/开机 -> 保持 Xbox -> 保持原 bond -> 自动重连原 PC
```

不得再次清 bond、不得自动进入配对、不得回退 PS。

PS 模式同理。

### 3.3 USB 与 BLE 配置必须独立

- USB 控制器类型：`GamepadOptions.inputMode`
- BLE 控制器类型：
  `AddonOptions.fightpadESP32ProxyOptions.bluetoothProfile`

USB 是 PS5 不代表 BLE 必须是 PS5。开机不得使用 USB `inputMode` 覆盖 BLE Profile。

## 4. 已确认的根因

### 4.1 蓝牙菜单只在 RAM 中生效，Flash 保存可能被拒绝

当前代码：

```text
src/addons/scrollwheel_menu.cpp:506-516
```

菜单选择 BLE Profile 后设置：

```cpp
options.has_bluetoothProfile = true;
options.bluetoothProfile = selectedProfile;
EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(false));
```

`false` 最终进入：

```text
src/gp2040.cpp:667-686
src/storagemanager.cpp:41-54
```

`Storage::save(false)` 在以下条件成立时直接返回 `false`：

- USB host 已启用；
- 当前 Driver 是 PS4/PS5；
- `getDongleAuthRequired()` 为 true。

PS5 通用驱动的认证要求为 true：

```text
src/drivers/p5general/P5GeneralDriver.cpp:100-102
```

因此当用户处于 PS5 USB 环境时，菜单中的 Xbox 选择可能只更新 RAM；当次 C6 收到
Xbox，所以看似切换成功，但 RP2350 断电重启后从 Flash 重新读出旧 PS 配置。

### 4.2 保存失败被静默忽略

当前 `GP2040::checkSaveRebootState()`：

```cpp
Storage::getInstance().save(forceSave);
```

没有使用返回值。结果是用户看不到保存失败，调用方也继续把 RAM Profile 同步给 C6。

### 4.3 启动时旧 PS 配置触发 C6 的真实 Profile 切换

RP2350 启动同步实现位于：

```text
src/addons/fightpad_esp32_proxy.cpp
```

相关函数：

- `getConfiguredBluetoothProfile()`
- `beginBluetoothProfileRequest()`
- `sendBluetoothProfileModeFrame()`
- `updateBluetoothProfileSync()`

当 RP2350 Flash 仍保存 PS 时，重启会发 Profile 3。C6 若已保存 Xbox，会把它判断为
Xbox -> PS 切换，按协议清旧 bond 并打开配对。这解释了：

```text
Xbox配对成功 -> 关机重启 -> 再次配对 -> 最终显示PS
```

## 5. 串口协议边界

RP2350 -> ESP32-C6 使用 UART0 固定 8 字节 XOR 帧：

```text
GPIO16/GPIO17（ESP32-C6 UART0）
```

Profile Mode：

```text
46 4D version profile sequence flags reserved checksum
```

当前 RP2350 正常同步只应设置：

```text
FIGHTPAD_BLE_PROFILE_FLAG_APPLY_NOW = 0x01
```

普通开机同步不要设置：

```text
FIGHTPAD_BLE_PROFILE_FLAG_FORCE_REPAIR = 0x02
```

Profile 编号：

| 值 | Profile |
|---:|---------|
| 0 | Generic |
| 1 | Xbox |
| 2 | Keyboard |
| 3 | PS5 PC |

## 6. 必须修改的 RP2350 代码

### 6.1 强制保存 BLE Profile，但不要重启 RP2350

修改：

```text
src/addons/scrollwheel_menu.cpp
```

蓝牙 Profile 属于明确的用户配置操作，应绕过 PS4/PS5 USB 认证期间的普通保存保护。
建议将当前调用改为等价的：

```cpp
new GPStorageSaveEvent(true, false)
```

含义：

- `forceSave=true`：确保 Profile 写入 RP2350 Flash；
- `restartAfterSave=false`：不重启 RP2350；C6 自己处理实际 Profile 变化所需的重启。

不要修改全局 `Storage::save(false)` 策略；只对 BLE Profile 这个明确配置动作强制保存，
避免影响 PS4/PS5 认证期间其他普通保存行为。

### 6.2 审计并修复 protobuf 有效位

确认以下两项同时落盘：

```cpp
options.has_bluetoothProfile = true;
options.bluetoothProfile = selectedProfile;
```

确认 Nanopb 生成/构建链会由：

```text
proto/config.proto
```

生成包含 `has_bluetoothProfile` 的配置结构。不要只修改运行时 RAM，不要只改 proto 默认值。

### 6.3 保存成功后再允许同步，或至少让失败可见

当前 Event 是异步保存，菜单修改 RAM 后 `FightpadESP32ProxyAddon` 可能立即向 C6 同步。
请审计 Event/Storage 架构并选择最小安全实现，满足：

1. 保存成功：允许向 C6 同步目标 Profile。
2. 保存失败：不能在用户无感知的情况下把仅存在 RAM 的 Profile 发给 C6。
3. 保存失败必须可追踪，至少有不刷屏的日志；若现有 OLED 事件结构允许，显示
   `Save Failed` 或等效错误。

如果为了保持现有架构只能先强制保存，请明确记录保存结果可视化是否尚未完成，不能
宣称已经支持失败反馈。

### 6.4 开机同步不得从 USB InputMode 派生 BLE Profile

核对 `getConfiguredBluetoothProfile()` 只能读取：

```cpp
Storage::getInstance().getAddonOptions()
    .fightpadESP32ProxyOptions.bluetoothProfile
```

不得根据以下值改写 BLE Profile：

```cpp
GamepadOptions.inputMode
DriverManager::getInputMode()
OLED当前USB标签
```

### 6.5 增加必要但不刷屏的日志

至少能够追踪一次选择和一次启动：

```text
BLE Profile menu selected: profile=1 has=1
BLE Profile save requested: force=1 restart=0
BLE Profile save result: success/failure
BLE Profile boot config: has=1 profile=1
BLE Profile Mode TX: profile=1 seq=N flags=0x01
BLE Profile ACK: accepted=1 result=...
```

如项目没有统一日志宏，可使用现有项目调试机制；不要每个主循环刷日志。

## 7. C6 侧已知边界，RP2350 不要用 workaround 掩盖

ESP32-C6 当前普通启动默认行为已经是：

- 缺失/无效 Profile NVS 默认 Xbox；
- `pending=0`；
- 不自动打开配对；
- 有 bond 时自动重连。

C6 仍有一个 ACK 语义需要 C6 侧后续处理：开机收到“相同 Profile”时当前可能回复
`ApplyingAtBoot`，而 RP2350 将 `ApplyingAtBoot` 显示为 `Pair Again`。这可能造成 OLED
看起来进入配对、但 C6 实际并未打开配对窗口。

RP2350 侧请：

- 记录 ACK 原始 result；
- 不要擅自把所有 `ApplyingAtBoot` 都当成 bond 已清除；
- 不要为了隐藏 OLED 现象跳过 Profile 同步；
- 不要自行清 C6 bond。

该 ACK 精确语义由 C6 工程修复和验证。

## 8. 明确禁止事项

1. 不要把 BLE Profile 再次合并进 USB `inputMode`。
2. 不要在 RP2350 普通开机时发送 `FORCE_REPAIR`。
3. 不要每次启动把 Profile 重设为 proto 默认值。
4. 不要因为用户当前 USB 是 PS5，就把 BLE Profile 改成 PS5。
5. 不要修改 ESP32-C6 GPIO13、配对窗口或 bond 数据。
6. 不要清理或恢复 RP2350 当前未提交的其他功能改动。
7. 按项目约定不要替用户编译/烧录 GP2040-CE；只做源码修改和静态验证。

## 9. 静态验证要求

至少完成：

1. `git diff --check`。
2. 搜索确认 BLE Profile 保存调用为强制保存且不请求 RP2350 重启。
3. 搜索确认 `has_bluetoothProfile` 与 `bluetoothProfile` 同时设置。
4. 搜索确认启动同步不读取 `GamepadOptions.inputMode` 作为 BLE Profile。
5. 固定 UART 向量检查：Xbox Mode 帧 profile=1、flags=0x01，checksum 正确。
6. 检查普通启动路径绝不设置 `FORCE_REPAIR`。

## 10. 人工实机验收矩阵

### 场景 A：PS -> Xbox

1. 当前 PS 已配对。
2. 菜单选择 Xbox。
3. RP2350 Flash 保存 Xbox 成功。
4. C6 允许重启一次并进入一次配对。
5. Windows 删除/更新旧设备并完成 Xbox 配对。

### 场景 B：Xbox 普通重启

1. Xbox 已成功配对。
2. 整机断电并重新开机。
3. RP2350 从 Flash 读出 Xbox。
4. RP2350 发送 Profile 1，flags 只能是0x01。
5. C6 保持 Xbox，不清 bond、不进入配对。
6. 自动重连原 PC。
7. OLED 保持 `XINPUT`。

### 场景 C：配对窗口超时

1. 手动按 ESP32-C6 GPIO13 打开配对窗口但不连接。
2. 30秒超时后退出配对状态。
3. Profile 仍为 Xbox，不得变成 PS。
4. 原有 bond 若未发生明确 Profile 切换，不得被清除。

### 场景 D：USB/BLE独立性

1. USB 模式保存为 PS5。
2. BLE Profile 保存为 Xbox。
3. 重启后 USB 仍显示/枚举 PS5。
4. 切换到蓝牙传输后 BLE 仍显示/枚举 Xbox。

## 11. 完成后回复模板

请 RP2350 侧 AI 完成后按以下格式回复：

```text
审计结论：
- RP2350 Flash中旧值为什么会保留：
- 是否确认Storage::save(false)被PS4/PS5认证条件拒绝：
- 启动同步最终读取的数据源：

修改内容：
- 文件：
- 函数：
- 保存调用最终参数：
- 保存失败如何处理/记录：

协议确认：
- Xbox开机Mode帧：
- flags：
- 是否存在FORCE_REPAIR：

验证结果：
- git diff --check：
- 静态测试：
- 未执行的构建/烧录：

需要用户实测：
- PS->Xbox首次重新配对：
- Xbox普通重启自动重连：
- 30秒配对超时后保持Xbox：
- USB PS5与BLE Xbox独立持久化：
```

## 12. 当前最可能的最终结论

当前现象不是“ESP32-C6 每次开机无条件自动配对”。第一次开机不配对已经证明普通 C6
启动路径有效。主要故障链是：

```text
RP2350菜单Xbox只改RAM
-> PS5 USB环境下save(false)拒绝写Flash
-> 重启加载旧PS
-> RP2350发送PS给C6
-> C6判断为真实Profile变化
-> 清bond并要求重新配对
```

必须先修复 RP2350 的持久化与启动同步，才能正确评价 C6 的普通重连。

# ESP32-C6 GPIO13 配对按键回归修复任务书

文档状态：可直接交给 ESP32-C6 工程 AI 执行  
问题：增加多 BLE Profile 后，按下蓝牙配对键没有稳定进入 `Pairing`  
RP2350 工程：`E:\ComporyProject\aa\GP2040-CE`  
ESP32-C6 权威工程：`E:\WorkSpace\C_WorkSpacee\ESP-IDF5.2\.espressif\release-v5.2\esp32c6_ble_hid_gamepad_test`

## 0. 给执行 AI 的直接指令

请只在上面的 ESP32-C6 权威工程中修复 GPIO13 显式配对路径。不要修改
RP2350 工程，不要改变现有 `FP/FT/FB/FI/FS/FM/FA` 8 字节 XOR 协议、Profile
编号、30 秒窗口或“普通开机只重连”的产品规则。

先读当前 `main/main.c`，再按本文的日志分支定位。不要通过恢复“每次开机自动
普通广播/自动配对”掩盖问题。

## 1. 已确认的双端事实

### RP2350 端不需要源码补丁

- GPIO13 配对键直接属于 ESP32-C6，不经过 RP2350 按键扫描。
- RP2350 已能解析 C6 的 `FS` 帧，`status=0x03` 会发布 `Pairing` 状态。
- OLED 已有居中的 `Pairing` 页面，蓝牙临时灯效也使用同一状态事件。
- RP2350 的 `FM` Profile 同步只负责 Profile 选择，不负责物理 GPIO13 按键。

因此，只有在串口已经证实 C6 发出了合法 `FS 03`、RP2350 仍无显示时，才重新
调查 RP2350；在此之前不要修改 RP2350。

### C6 当前代码中的回归风险

当前 `main/main.c` 有以下关键路径：

1. `pair_button_task()` 对 GPIO13 做 30 ms 消抖，按下后调用
   `trigger_pairing_mode()`。
2. `trigger_pairing_mode()` 调用 `open_pairing_window(true)`，随后只设置异步
   `request_link_termination()`。
3. `update_ble_status_output()` 当前先判断 `state.hid_connected`，之后才判断
   `pairing_status_active()`。
4. 因此设备仍处于 HID Connected 时按下配对键，刚打开的 Pairing 状态会被
   Connected 遮住；如果断开状态传播异常，OLED 可以一直看不到 Pairing。
5. GPIO13 配置为内部上拉，但代码把启动瞬间读到的电平保存成
   `s_pair_button_idle_level`。如果上电瞬间按键被按住或电平尚未稳定，后续电平
   方向可能被反向解释。
6. `BLE_PROFILE_FLAG_FORCE_REPAIR` 当前只打印 `no effect in v1`。这不是物理
   GPIO13 无响应的直接原因，但属于未完成的协议行为。

## 2. 必须完成的最小修复

### A. Pairing 状态优先于 Connected

把 `update_ble_status_output()` 的优先级改为：

```c
if (pairing_status_active()) {
    status = BLE_STATUS_PAIRING;
} else if (state.hid_connected) {
    status = BLE_STATUS_CONNECTED;
} else if (connection_active || high_duty_directed) {
    status = BLE_STATUS_CONNECTING;
} else {
    status = BLE_STATUS_DISCONNECTED;
}
```

这是本轮必修项。显式配对窗口是用户主动操作，在完整 30 秒窗口内必须覆盖旧的
Connected 状态。

### B. 按键触发后立即发送 Pairing

在 `trigger_pairing_mode()` 中，成功通过 BT Transport 判断后保持以下顺序：

```c
open_pairing_window(true);
send_ble_status_frame(BLE_STATUS_PAIRING);
request_link_termination();
```

立即发送用于缩短 OLED 反馈延迟；A 项的优先级调整用于保证后续 10 ms 循环不会
马上把状态改回 Connected。第二次按下只重新开始 30 秒计时，不得取消配对。

### C. 保证已连接链路真的断开

保留当前单一 GAP 断链管理，不要在多个任务直接同时调用
`ble_gap_terminate()`。确认 `request_link_termination()` 设置的标志会由
`manage_advertising()` 消费，并在已有连接句柄时调用：

```c
ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
```

断开完成前保持 `Pairing` 状态；断开后在窗口未过期时启动普通快速广播，不得又
立即回到旧绑定设备的定向广播。

### D. 不要在物理按键路径立即清空全部 bond

继续保留当前策略：GPIO13 只打开显式配对窗口并断开旧链路，不在按钮处理函数中
直接调用全局 `ble_store_clear()`。旧主机重复配对由现有
`BLE_GAP_EVENT_REPEAT_PAIRING` 路径处理，避免 C6 与主机两端 bond 不一致。

## 3. GPIO13 没有产生按下事件时的修复分支

先观察日志，只有看不到下面第一条日志时才进入本节：

```text
pair button debounced: pressed
```

用万用表或启动日志确认 Fightpad12Slim 实机 GPIO13：释放时应为高，按下时应为
低。若确认如此，停止使用启动瞬间的“学习空闲电平”，改为明确的低有效定义：

```c
#define PAIR_BUTTON_ACTIVE_LEVEL 0

bool raw_pressed =
    gpio_get_level(PAIR_BUTTON_GPIO) == PAIR_BUTTON_ACTIVE_LEVEL;
```

并删除 `s_pair_button_idle_level` 的运行时学习逻辑。保留内部上拉和 30 ms 消抖。
如果实测电路是释放低、按下高，则使用实测极性，不能只凭猜测修改。

为避免“上电时一直按住”被当成一次新按下，可要求任务启动后先观察到一次释放态，
再允许发布 pressed 边沿；但正常运行中的一次按下必须可靠触发。

## 4. Transport 分支检查

如果日志出现：

```text
pair button ignored: transport mode disables bluetooth
```

说明按键检测正常，失败原因是 C6 当前收到或保留了 USB Transport。此时不要改
GPIO；检查 RP2350 的 `FT` 帧和 GP33/GP34 实际挡位。BT 挡下应看到：

```text
transport mode: bluetooth
```

本轮产品规则仍允许 USB Transport 下忽略配对键；验收必须在实体 BT 挡进行。

## 5. FORCE_REPAIR 协议补全

此项不能替代物理按键修复，但建议同时补完，避免以后 UI 或维护命令调用时仍然
无效：

- Runtime 收到相同 Profile 且带 `BLE_PROFILE_FLAG_FORCE_REPAIR`：回复与当前
  v1 兼容的 ACK，打开 30 秒显式配对窗口，立即发送 `FS Pairing`，请求断开旧
  链路；不因为 Profile 相同而重启整机。
- Boot 阶段收到该 flag：保存/保留 `BLE_PROFILE_PENDING_PAIRING`，等 HID host
  ready 后由现有启动路径打开窗口。
- 未携带该 flag 的相同 Profile：保持现状，只重连，不重新配对。
- 如果本轮暂不实现，必须保留明确 TODO，不能继续把它描述成已支持。

当前 RP2350 固件只发送 `APPLY_NOW`，没有发送 `FORCE_REPAIR`，所以即使本项未
完成，也不能作为 GPIO13 无响应的解释。

## 6. 必须增加的诊断日志

一次 GPIO13 按下至少应依次得到可区分的日志：

```text
pair button debounced: pressed
pair button pressed: opening/restarting 30000 ms pairing window
BLE status pairing ...
BLE disconnect reason=...          # 仅原来已连接时
advertising as ... (fast interval)
```

如果状态发送函数目前只打印数字，应让日志同时显示 `pairing`，便于区分 UART 状态
与 BLE GAP 状态。不要在 10 ms 循环持续刷同一条日志。

## 7. 实机验收

以下用例全部通过才算修复完成：

1. BT 挡、未连接：短按一次 GPIO13，100 ms 内 OLED 居中显示 `Pairing`，开始
   30 秒普通快速广播。
2. BT 挡、已连接：短按一次，OLED 立即显示 `Pairing`，旧连接被主动断开，随后
   进入 30 秒普通快速广播。
3. Pairing 期间再次按下：窗口重新计时 30 秒，不取消、不闪回旧页面。
4. 30 秒到期：停止允许陌生主机连接；有旧 bond 时恢复定向重连，无 bond 时停止
   普通广播。
5. Profile 不变的普通断电重启：只重连最近绑定主机，不自动重新配对。
6. Generic、Keyboard、Xbox Layout、PlayStation Layout 四个 Profile 都重复
   用例 1～5，物理按键行为一致。
7. 保存一段完整串口日志，必须能从按键边沿追踪到 `FS 03`、断链、快速广播及
   30 秒到期。

## 8. 执行 AI 完成后的回复格式

```text
已确认工程路径：...
根因：...
修改文件与函数：...
GPIO13实测极性：释放=...，按下=...
Pairing/Connected状态优先级：...
FORCE_REPAIR：已实现 / 本轮未实现及原因
构建结果与固件路径：...
实机日志：...
7项验收结果：...
```

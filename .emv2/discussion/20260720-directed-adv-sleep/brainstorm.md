# 头脑风暴: 定向广播 + 断连空闲降速

## 难点总结

| 难点 | 风险 | 决策 |
|------|------|------|
| 定向广播 1.28s burst 管理 | 高 | 利用 NimBLE duration 参数自动超时，ADV_COMPLETE 后切回普通广播 |
| Peer 地址保存时机 | 低 | BLE_GAP_EVENT_CONNECT 时从 desc 读取 |
| 定向广播→普通广播的回退 | 中 | ADV_COMPLETE 根据 s_adv_directed 决定下一步 |
| 断连后多个定时器（1.5s延迟、1min超时、1.28s burst） | 高 | 统一由 pair_button_task 管理，避免 gap_event 内直接操作 |
| 定向广播与 pair_button_task 冲突 | 中 | pair_button_task 检测到定向模式时跳过 keepalive |
| 按键在 burst 中再次触发 | 低 | 每次按键都重启定向 burst（停当前广播 → ADV_COMPLETE → 重启定向） |
| 慢广播降速触发条件 | 低 | 断连 1min 未重连 → 固定 200ms；有按键活动 → 立即定向 burst |
| directed_addr 为空时怎么处理 | 低 | 首次配对前无 peer 地址 → 跳过定向广播直接用快广播 |
| 定向广播期间其他设备可以发现吗 | 无 | BLE 规范限制：定向广播只有目标设备能看到。这是预期行为 |
| 配对按键(GPIO13)触发时 | 低 | trigger_pairing_mode() 清 bond 后 peer addr 也清掉，不做定向 |

## riority-ordered 改动

### 1. 保存配对主机地址 (新增变量)

```c
static ble_addr_t s_peer_addr;        // 上次配对/连接的主机地址
static bool s_peer_addr_valid;        // 地址是否有效
```

CONNECT 成功时从 `desc.peer_id_addr` 拷贝；GPIO13 清 bond 时清 `s_peer_addr_valid = false`。

### 2. 定向广播控制 (新增变量 + 函数)

```c
#define DIRECTED_ADV_TIMEOUT_MS 1280  // BLE spec max for HD directed adv
static bool s_adv_directed;           // 当前是否为定向广播 burst
```

新增 `start_directed_advertising()`: 用 `BLE_GAP_CONN_MODE_DIR` + `high_duty_cycle=1` + `duration_ms=1280` + `direct_addr=&s_peer_addr` 调用 `ble_gap_adv_start()`。

### 3. ADV_COMPLETE 处理定向回退

ADV_COMPLETE 触发时：
- `s_adv_directed` → 切回普通广播（fast undirected），发送 0x00 状态
- 普通广播超时 → 继续普通广播

### 4. UART 活动触发定向广播

`handle_uart_frame()` 收到手柄报告时：
- `s_conn_handle == NONE` 且 `s_peer_addr_valid` → 停止当前广播 → ADV_COMPLETE → 启动定向广播

### 5. 断连 1min 降速 + 按键恢复

pair_button_task 新增逻辑：
- `(now - s_disconnect_tick) >= 1min` 且非定向模式 → 普通广播降为固定 200ms
- UART 活动 → 跳出慢广播 → 或跳入定向 burst

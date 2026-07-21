# 子流程拆分: 定向广播 + 断连空闲降速

## 改动概述

```
断连 → 1.5s 延迟 → 快广播(30-50ms)
  │
  ├── UART 按键活动 → 定向广播 burst(1.28s) → 回退到快广播
  │
  ├── 1min 无重连 → 慢广播(200ms 固定)
  │     └── UART 按键活动 → 定向广播 burst(1.28s) → 回退到慢广播
  │
  └── 重连成功 → 停止广播
```

所有改动均在 `main/main.c`。

---

## S4-A: 保存配对主机地址 + 定向广播基础结构

- **所属子系统**: 定向广播触发
- **开发内容**:
  1. 新增变量: `s_peer_addr` (ble_addr_t), `s_peer_addr_valid` (bool), `s_adv_directed` (bool)
  2. `BLE_GAP_EVENT_CONNECT` status=0 时从 `desc.peer_id_addr` 保存地址
  3. `trigger_pairing_mode()` 清 bond 时清 `s_peer_addr_valid = false`
  4. 新增 `start_directed_advertising()`: BLE_GAP_CONN_MODE_DIR + high_duty_cycle=1 + duration_ms=1280 + direct_addr=&s_peer_addr
  5. `start_advertising()` 增加定向广播参数支持（或拆分为独立函数）
- **前置条件**: 无
- **验证方式**: 连接 → 断连 → 检查日志确认 peer addr 已保存
- **优先级**: 1

## S4-B: 断连 1 分钟降速到 200ms 慢广播

- **所属子系统**: 断连空闲超时→慢广播
- **开发内容**:
  1. 新增 `#define DISCONNECT_SLOW_ADV_MS 60000` (1 分钟)
  2. 修改 `pair_button_task` 保活 + 动态间隔逻辑：
     - 移除 60s UART 无活动 → idle (100-200ms) 的动态切换
     - 改为：断连 1min 未重连 → 固定 200ms 广播
     - 复用 `s_disconnect_tick` 作为计时起点
  3. `start_advertising()` 根据当前模式（快/慢/定向）选不同参数
- **前置条件**: S4-A 完成（共用广播参数选择逻辑）
- **验证方式**: 断连后等待 1 分钟，确认广播间隔变为 200ms
- **优先级**: 2

## S4-C: UART 按键活动 → 定向广播 burst

- **所属子系统**: RP2350 活动检测增强
- **开发内容**:
  1. `handle_uart_frame()` 收到手柄报告时：
     - 如果 `s_conn_handle == NONE` 且 `s_peer_addr_valid`
     - → 设 `s_adv_directed = true` → 停当前广播 → ADV_COMPLETE 后启动定向
  2. ADV_COMPLETE 处理：`s_adv_directed` → 切回快/慢广播（根据 1min 计时器）
  3. 每次按键都触发新的 1.28s burst（即使在 burst 中也重启）
- **前置条件**: S4-A, S4-B 完成
- **验证方式**: 断连后按键 → 日志确认进入定向广播 → 1.28s 后切回普通广播
- **优先级**: 3

## S4-D: 状态机清理 + 边界条件

- **所属子系统**: 全部
- **开发内容**:
  1. `s_adv_directed` 期间，保活逻辑跳过
  2. BLE_GAP_EVENT_CONNECT status≠0 时也停止定向广播标记
  3. 首次配对前无 peer addr → 跳过定向广播
  4. 整体编译 + 烧录验证
- **前置条件**: S4-A, S4-B, S4-C 完成
- **优先级**: 4

# 需求拆分: 蓝牙连接状态弹窗与 Base 灯效

讨论ID: `2026-07-20-bluetooth-status-popup-led`

## 1. UART 状态帧接收

- 类型: 通信 / 状态机
- 简述: 复用 RP2350B UART0 GP44/GP45，在现有 `0x46 0x49` 固件信息帧之外接收 `0x46 0x53` 蓝牙状态帧。

## 2. 跨核状态事件

- 类型: 数据管理
- 简述: Core0 校验并发布时间戳状态事件，供 Core1 OLED 和 Core0 Base 灯效读取。

## 3. OLED 临时状态页

- 类型: 显示
- 简述: 收到状态事件时覆盖当前页面，Connecting/Pairing 持续显示，Connected/Disconnected 显示 1 秒后恢复弹窗前页面。

## 4. GP40 Base 临时灯效

- 类型: 灯光 / 状态机
- 简述: Connecting/Pairing 使用纯蓝 Chase，Connected 使用全蓝静态灯 1 秒，Disconnected 将 Base 置黑 1 秒，随后恢复原灯效。

## 5. 验证

- 类型: 测试
- 简述: 执行协议样例、状态期限、灯效优先级和非构建静态检查，随后由用户完成构建与实机验证。

## 用户确认

用户确认上述拆分，并确认状态灯效可在 `All OFF` 时临时唤醒 GP40，但不得越过 7% 低电保护。

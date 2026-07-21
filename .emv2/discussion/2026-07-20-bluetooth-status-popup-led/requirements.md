# 需求确认: 蓝牙连接状态弹窗与 Base 灯效

讨论ID: `2026-07-20-bluetooth-status-popup-led`

## UART 协议

- ESP32-C6 到 RP2350B 单向通知，RP2350B 不回复。
- 固定 8 字节帧: `46 53 status 00 00 00 00 checksum`。
- `checksum` 为 Byte0 至 Byte6 的 XOR。
- 状态仅接受 `0x00=Disconnected`、`0x01=Connecting`、`0x02=Connected`、`0x03=Pairing`；其他值忽略。
- 复用 UART0、115200 8N1、GP44=TX、GP45=RX，不影响现有 `0x46 0x49` 固件信息帧。
- ESP32 仅在状态变化时发送；RP2350 不增加轮询、回复或自动断连超时。

## OLED 行为

- `Connecting`: 收到 `0x01` 后立即唤醒 OLED 并显示临时蓝牙页面，直到收到最终状态。
- `Connected`: 收到 `0x02` 后显示 1 秒。
- `Disconnected`: 收到 `0x00` 后显示 1 秒；协议不区分连接失败和普通断连。
- `Pairing`: 收到 `0x03` 后立即显示 `Pairing...`，持续到 ESP32 发来下一状态。
- 结果页到期后撤销覆盖，恢复弹窗前页面和菜单状态。
- 页面使用当前 ASCII 字体显示 `Bluetooth Status`、`Connecting...`、`Connected`、`Disconnected`、`Pairing...`。

## Base 灯效行为

- 仅覆盖 GP40 的 19 颗 Base LED，不改变 GP22 Key Effect/Key Flash。
- `Connecting`: 5 颗纯蓝梯度 Chase，持续到下一状态。
- `Pairing`: 与 Connecting 相同，使用 5 颗纯蓝梯度 Chase，持续到下一状态。
- `Connected`: 19 颗灯全蓝静态显示 1 秒。
- `Disconnected`: 19 颗 Base LED 全黑 1 秒。
- 覆盖结束后恢复当前 Base 效果、颜色、亮度和动画状态，不写 Flash。
- 状态灯效可临时越过菜单 `All OFF` 并唤醒 GP24/VCC_5V；覆盖结束后恢复原关断状态。
- `SOC <= 7%` 低电关灯始终具有更高优先级，禁止状态灯效点亮。

## 用户确认

用户输入“同意，继续”，确认以上需求。

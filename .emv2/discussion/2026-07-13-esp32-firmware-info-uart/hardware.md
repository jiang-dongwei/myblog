# 硬件对齐: ESP32-C6 固件信息串口接收与菜单显示

讨论ID: `2026-07-13-esp32-firmware-info-uart`

## 已有硬件连接

| RP2350B | 方向 | ESP32-C6 |
|---|---:|---|
| GP44 / UART0 TX | -> | GPIO17 / UART0 RX |
| GP45 / UART0 RX | <- | GPIO16 / UART0 TX |
| GND | - | GND |

## 配置结论

- 逻辑电平: 3.3V TTL。
- UART: 115200、8N1、无流控。
- GP44、GP45 已在 `BoardConfig.h` 中标记为 `ASSIGNED_TO_ADDON`。
- GP36~GP39 属于另一组预留 UART1/流控引脚，本功能不使用。
- 不需要新增引脚、中断、DMA、CTS 或 RTS。
- 不使用 GP34/GP35 主动控制 ESP32 RESET/BOOT。
- 接收行为不依赖 GP33 当前选择 USB 还是蓝牙。

## 上电同步

- 依靠 ESP32-C6 UART 初始化后的既有发送时序，不增加同步线。
- 不主动复位 ESP32-C6，不扩展请求重发协议。
- RP2350B 如果漏掉启动序列，菜单保持显示 `Coming to soon`，等待下次共同上电。

## 用户确认

用户输入 `继续`，确认硬件对齐通过。

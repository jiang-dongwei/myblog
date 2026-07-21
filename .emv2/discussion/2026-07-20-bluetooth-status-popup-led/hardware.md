# 硬件对齐: 蓝牙连接状态弹窗与 Base 灯效

讨论ID: `2026-07-20-bluetooth-status-popup-led`

## UART

| RP2350B | 方向 | ESP32-C6 |
|---|---:|---|
| GP44 / UART0 TX | -> | GPIO17 / UART0 RX |
| GP45 / UART0 RX | <- | GPIO16 / UART0 TX |
| GND | - | GND |

- 逻辑电平: 3.3V TTL。
- 参数: 115200、8N1、无流控。
- 不新增引脚、中断、DMA、CTS 或 RTS。

## 显示与灯链

- OLED: 现有 SSD1306/GPGFX 显示链路，由 Core1 `DisplayAddon` 绘制。
- Base: GP40 的 19 颗 WS2812，由 Core0 `FightpadAmbientLEDAddon` 唯一写入。
- Key: GP22 的 12 颗 WS2812，不参与本次状态覆盖。
- 电源: GP24 控制 RGB 5V；Connecting/Pairing/Connected 可临时唤醒，Disconnected 与覆盖结束按原菜单状态处理。
- 低电保护: BQ27220 有效 SOC 小于等于 7% 时保持 GP40/GP22 全黑并关闭 RGB 电源，优先于本功能。

## 用户确认

用户确认不增加硬件，并允许蓝牙状态提示在 `All OFF` 时临时点亮 GP40。

# 硬件与协议对齐：Xbox Layout BLE HID

## 板间通信

| 项目 | 固定值 |
|---|---|
| ESP32-C6 UART | UART0 |
| TX | GPIO16 |
| RX | GPIO17 |
| 参数 | 115200, 8N1 |
| 帧长度 | 8 bytes |
| 校验 | byte0..6 XOR 存入 byte7 |

UART0 GPIO16/GPIO17 专用于 RP2350 二进制协议。当前基线同时把 ESP-IDF Console
配置到 UART0，存在污染 `FM`、`FA`、`FS` 帧的风险；实现时将主 Console 切到
USB Serial/JTAG，不把板间协议迁移到 UART1。

## Profile v1 协议

- Xbox Profile ID：`1`。
- Mode type：`M` (`0x4D`)，方向 RP2350 -> C6。
- ACK type：`A` (`0x41`)，方向 C6 -> RP2350。
- `APPLY_NOW` flag：`0x01`。
- 固定 Xbox Mode 向量：`46 4D 01 01 2A 01 00 20`。
- 固定 Restarting ACK：`46 41 01 01 2A 01 00 2C`。
- 固定 Unchanged ACK：`46 41 01 01 2A 00 00 2D`。

## 启动与重启

1. 初始化 NVS 和 UART0。
2. HID 初始化前等待约 500 ms Profile 同步。
3. Profile 改变时保存 NVS 与 pending 标志。
4. ACK 完整发送后延迟约 50 ms，仅重启 ESP32-C6。
5. 新启动选择对应 HID descriptor，清旧 bond，并打开 30 秒配对窗口。

## 其他硬件

- GPIO13 继续作为 ESP32-C6 配对键；本步骤不改变引脚。
- 不新增 GPIO、PWM、DAC、中断或 DMA。
- 不新增振动电机或振动通信通道。

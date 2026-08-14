# 硬件对齐

- 目标芯片：ESP32-C6，仅支持 BLE，不使用 Classic Bluetooth/BR/EDR。
- 配对按键：ESP32-C6 GPIO13，保持现有中断、消抖和有效电平逻辑。
- RP2350 通信：ESP32-C6 UART0，GPIO16/GPIO17，帧协议不变。
- 本步骤没有新增引脚、电平、外设、中断或 DMA 需求。
- 不修改 `E:\ComporyProject\aa\GP2040-CE`。

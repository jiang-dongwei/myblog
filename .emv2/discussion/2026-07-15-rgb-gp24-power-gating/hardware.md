# 硬件对齐

讨论ID：`2026-07-15-rgb-gp24-power-gating`

## 原理图证据

- 文件：`22-FIGHTPAD_20260625-schematic_new.pdf`，第1页。
- GPIO24 网名为 `5V_EN`，经 R76 10K 接 FP6276BXR-G1 的 EN。
- FP6276 输出网名为 `VCC_5V`，连接 GP22/GP40 两条 WS2812 灯链。
- RP2350/ESP32 的 3.3V 电源由 U79 SY8088AAC 从 VCC 生成，输出为独立的 `OVCC_3V3`。

## 时序与资源

- GP24 active-high，板级宏 `FIGHTPAD12SLIM_AMBIENT_BOOST_EN_LEVEL=1`。
- 关断前黑帧等待：1000us。
- 恢复上电等待：5ms。
- 不增加 PIO、DMA、定时器、UART 或 I2C 资源。

## 实机检查

- 分别在 USB 与电池供电下选择 `All OFF`，确认设备本身不复位、GP24为低、VCC_5V消失。
- 选择 Key/Base 动态效果或非黑颜色，确认 GP24恢复高电平且第一帧正常。
- 测量关灯前后整机电流差，并确认 GP22/GP40 在断电期间保持低电平、没有数据脚反向供电。

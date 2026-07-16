# 硬件边界

- 电量计：BQ27220，继续使用 GP25/GP26 软件 I2C，每 2000 ms 轮询。
- 电量日志：RP2350B UART1，GP42=TX、GP43=RX，115200 8N1。
- OLED：现有 I2C0 SSD1306，复用显示驱动 `setPower()`，不新增硬件控制脚。
- 唤醒输入：GP30、GP31、GP32；GP19 BACK 不计入本次 1 分钟空闲计时。


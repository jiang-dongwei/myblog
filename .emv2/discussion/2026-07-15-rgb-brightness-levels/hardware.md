# 硬件对齐

讨论ID：`2026-07-15-rgb-brightness-levels`

## 已有灯光硬件

- Key Effect：GP22，12 颗 WS2812，由 Fightpad 灯光 addon 使用 PIO2/SM1 驱动。
- Base Effect：GP40，19 颗 WS2812，由 Fightpad 灯光 addon 使用 PIO2/SM0 驱动。

## 已有菜单输入

- GP30：确认/长按输入。
- GP31、GP32：菜单上下选择。
- GP19：返回。

## 配置与资源

- 亮度设置复用现有 Flash 配置，不增加外部存储器。
- 亮度通过修改 `RGB::value()` 的浮点系数实现，不增加 PWM、定时器、DMA 或 PIO 状态机。
- 不修改 GPIO 分配、PIO 分配、LED 数量或灯光供电控制。
- 与 OLED、BQ27220、UART1 电量日志和 UART0 ESP32 通信没有外设资源冲突。
- 7% 低电保护继续在最终输出层强制清空 GP22/GP40 帧。

## 对齐结论

- 本功能是纯软件菜单、配置和渲染调整，无新增硬件需求。
- 用户确认现有硬件资源和控制范围正确。


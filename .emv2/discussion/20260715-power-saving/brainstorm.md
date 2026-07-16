# 头脑风暴 — 降低功耗

## 难点 1: Auto Light Sleep 与 BLE/NimBLE 的兼容性

**分析**: NimBLE controller 有自己的定时器来控制 RF 收发。如果 BLE 正在连接/广播，controller 会频繁唤醒 CPU，light sleep 基本睡不深。但在空闲态（无连接无广播），controller 没有活动，light sleep 可以深度休眠。

**方案**: 不手动管理 light sleep 进入/退出，使用 ESP-IDF 的 Auto Light Sleep（`CONFIG_PM_ENABLE=y`），由电源管理模块根据 idle 时间自动决策。

**确认**: ✅ 空闲态生效，连接态自动降级为浅睡或不睡。

## 难点 2: UART 唤醒可靠性

**分析**: Light Sleep 时 UART 时钟关闭，需要配置硬件唤醒阈值。如果阈值太低（1 字节），可能被噪声唤醒；如果太高，可能丢数据。

**方案**: 设 `uart_set_wakeup_threshold(UART_NUM_0, 3)`。UART 控制器检测到 3 个字节后才唤醒 CPU。RP2350 发的是 8 字节帧（0x46 开头），3 字节阈值为帧头提供了足够容错。

**确认**: ✅ 不丢数据，抗噪声。

## 难点 3: GPIO13 从轮询改为中断

**分析**: 当前 pair_button_task 是 10ms 轮询 + 防抖。如果换成 GPIO 边沿中断 + RTC GPIO 唤醒，可以完全省掉这个任务在空闲时的 CPU 唤醒。

**方案**: 
- GPIO13 配置 `GPIO_INTR_NEGEDGE`（下降沿中断）
- 中断中不直接处理，用 FreeRTOS 任务通知唤醒 pair_button_task
- pair_button_task 改为阻塞等待通知（`ulTaskNotifyTake`），而不是周期性轮询
- 防抖逻辑保持不变

**确认**: ✅ 空闲时 zero 轮询开销，按键响应仍即时。

---
状态: ✅ 已完成

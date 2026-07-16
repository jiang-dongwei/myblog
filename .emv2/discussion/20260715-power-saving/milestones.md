# 子流程拆分 — 降低功耗

## S2-A: CPU 固定降频 80MHz
- **所属子系统**: CPU/SoC 电源管理
- **开发内容**:
  1. sdkconfig: `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ` 160 → 80
  2. sdkconfig: 确认 `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_80=y`
- **前置条件**: 无
- **验证方式**: 编译烧录后串口日志确认 CPU 频率
- **优先级**: 1

## S2-B: BLE 广播间隔加大
- **所属子系统**: BLE 功耗优化
- **开发内容**:
  1. main.c `start_advertising()`: `adv_params.itvl_min/MAX` 从 30-50ms → 100-200ms
- **前置条件**: 无
- **验证方式**: 编译烧录，用手机扫描 BLE 设备确认发现速度和正常连接
- **优先级**: 2

## S2-C: Auto Light Sleep + 空闲休眠
- **所属子系统**: CPU/SoC 电源管理 + FreeRTOS
- **开发内容**:
  1. sdkconfig: 启用 `CONFIG_PM_ENABLE`、`CONFIG_FREERTOS_USE_TICKLESS_IDLE`
  2. main.c: UART 配置唤醒阈值 `uart_set_wakeup_threshold(UART_NUM_0, 3)`
  3. main.c: 确保 Light Sleep 唤醒后 UART 正常工作
- **前置条件**: S2-A
- **验证方式**: 空闲状态下测电流（期望从 ~30mA 降到 ~1mA 级别），确认 UART 收发正常、GPIO13 唤醒正常
- **优先级**: 3

## S2-D: GPIO13 轮询改中断 + RTC GPIO 唤醒
- **所属子系统**: 外设功耗管理
- **开发内容**:
  1. main.c: GPIO13 增加 `GPIO_INTR_NEGEDGE` 中断注册
  2. main.c: pair_button_task 改为阻塞等待任务通知（`ulTaskNotifyTake`），去掉 10ms 轮询
  3. main.c: ISR 中做简单防抖（或用定时器防抖），然后通知 task
- **前置条件**: S2-C（依赖 light sleep 环境）
- **验证方式**: 空闲状态下 GPIO13 按键能正常唤醒并触发配对
- **优先级**: 4

---
状态: ✅ 待确认

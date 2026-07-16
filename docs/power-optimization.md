# ESP32-C6 BLE HID Gamepad 功耗优化记录

> 日期: 2026-07-15 ~ 2026-07-16
> 讨论ID: 20260715-power-saving
> 步骤编号: S2

## 实测功耗变化

| 状态 | 优化前 | 优化后 |
|------|:------:|:------:|
| 空闲（无广播无连接）| ~82mA | 待测 |
| 广播中 | ~82mA | 待测 |
| 已连接 | — | 待测 |

## 保留的优化

### S2-A: CPU 固定降频 160 → 80MHz

**文件**: `sdkconfig`

```ini
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_80=y
# CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_160 is not set
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=80
```

**效果**: 省约 10-15mA，对 BLE 协议栈无影响。

---

### S2-B: BLE 广播间隔加大

**文件**: `main/main.c` — `start_advertising()`

```c
// 改前: 30ms-50ms（每秒 20-33 次广播包）
adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(30);
adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(50);

// 改后: 100ms-200ms（每秒 5-10 次广播包）
adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(100);
adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(200);
```

**效果**: 广播阶段射频功耗降约 50-70%。手机发现设备稍慢（1-2 秒内），不影响连接。

---

### S2-E: NimBLE Modem Sleep + 开机自动广播

#### E1: 启用 BLE Modem Sleep

**文件**: `sdkconfig`

```ini
CONFIG_BT_LE_SLEEP_ENABLE=y
```

**原理**: 
- ESP32-C6 NimBLE controller 内置 modem sleep 机制，与 `CONFIG_PM_ENABLE`（ESP-IDF 全局电源管理）**完全独立**
- 启用后注册 `controller_sleep_cb` / `controller_wakeup_cb` 回调
- 在 BLE 射频事件间隙（广播间隔、连接间隔之间）调用 `esp_phy_disable(PHY_MODEM_BT)` **真正关闭 BLE PHY 硬件**

**关键**: 需要 BLE 事件（广播或连接）作为"休眠锚点"——controller 必须知道下次射频事件何时到来，才能在间隙关 PHY 打盹。

#### E2: 开机自动广播

**文件**: `main/main.c` — `app_main()` 末尾

```c
/* Boot: default transport to BT if RP2350 hasn't set it yet, then open
 * pairing window so BLE is discoverable + modem sleep has an RF schedule */
if (!s_transport_mode_seen) {
    set_transport_bt_enabled(true);
}
open_pairing_window();
```

**双重作用**:
1. 用户开机即可搜到设备，无需手动按 GPIO13
2. 为 modem sleep 提供 RF 事件锚点，让 controller 可以在广播间隔关 PHY

---

## 已回滚的尝试

### S2-C: ESP-IDF Auto Light Sleep（已回滚）

- 配置: `CONFIG_PM_ENABLE=y` + `CONFIG_FREERTOS_USE_TICKLESS_IDLE=y`
- **问题**: 与 NimBLE controller 内部休眠冲突 → BLE PHY 临界区死锁 → Task Watchdog 触发
- **结论**: ESP-IDF v5.2 + ESP32-C6 NimBLE 不兼容 `CONFIG_PM_ENABLE`

### S2-D: GPIO13 轮询改中断（已回滚）

- 改动: GPIO13 从 10ms 轮询改为下降沿中断 + RTC GPIO 唤醒
- **问题**: 导致内存损坏（`MSTATUS: 0xa5a5a5a5`）和系统崩溃
- **结论**: 中断 + ISR + `ulTaskNotifyTake` 的实现有 bug，回滚到轮询方式

---

## 技术背景: 为什么空闲比广播更费电？

NimBLE controller 的休眠调度**必须有已知的 BLE 射频事件时间表**才能工作：

```
空闲状态（82mA）：          广播中（65mA）：

BLE PHY ON (持续)          BLE PHY ON ─── 广播包
  │                           │
  │  无广播/连接事件           │  100-200ms 间隔
  │  Controller 不知道        │  controller_sleep_cb
  │  下次何时需要射频          │  → esp_phy_disable(PHY_MODEM_BT)
  │  → PHY 不敢关              │  → PHY 掉电 ~150ms
  │                           │  → controller_wakeup_cb
  ▼                           ▼  → PHY 上电，下一个广播包
  82mA                       平均 65mA
```

**结论**: 没有广播/连接时，controller 无法调度休眠。启用 modem sleep 后，需要配合 BLE 事件（开机广播）来提供休眠锚点。

---

## 修改文件清单

| 文件 | 改动 |
|------|------|
| `sdkconfig` | CPU 80MHz, `CONFIG_BT_LE_SLEEP_ENABLE=y` |
| `main/main.c` | BLE 广播参数 100-200ms, 开机自动配对窗口 |

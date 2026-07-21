# 记忆日志

## 2026-07-13: S1 固件信息UART发送 — 已完成

- 步骤编号: S1 (S1-A + S1-B)
- 讨论ID: 20260712-fw-info-uart
- 功能: 读取ESP32固件信息(SDK/Plat/Board/CPU)，通过UART(GPIO16/GPIO17)发送给RP2350

### 实现摘要

**修改文件**:
- `main/main.c`: 新增 `#include <stdio.h>`, `#include "esp_idf_version.h"`; 新增 FW_INFO 帧协议宏 (0x49 'I'), CPU 架构映射宏; 新增 `fw_info_build_payload()` + `send_fw_info_frames()` 函数; `app_main()` 中 UART 初始化后发送固件信息
- `main/CMakeLists.txt`: 添加 `PROJECT_NAME` 编译宏

**协议设计**:
- 帧类型: `I` (0x49), 8字节帧格式, XOR校验
- Byte2: flag(bits7-6) + seq(bits5-0), flags: 11=首帧, 10=中间帧, 01=尾帧, 00=单帧
- Byte3-6: 4字节payload数据
- Payload格式: `SDK=5.5.1\nPlat=esp32c6\nBoard=fightpad12slim_c6_ble_hid\nCPU=RISC-V\n`
- 发送时机: UART初始化后、C6_DONE之前立即同步发送

**字段来源**:
- SDK: `esp_get_idf_version()` 运行时API, 去掉 'v' 前缀
- Plat: `CONFIG_IDF_TARGET` 编译宏
- Board: `FW_INFO_BOARD_NAME` 宏 → `"Fightpad12Slim_C6_BLE_HID"`（规范化大小写）
- CPU: `CONFIG_IDF_TARGET_ARCH_RISCV` → "RISC-V"

---

## 2026-07-15: S2 降低功耗 — 已规划

- 步骤编号: S2 (S2-A + S2-B + S2-C + S2-D)
- 讨论ID: 20260715-power-saving
- 目标: 降低空闲/非连接状态功耗，不影响正常工作

### 子步骤

| 步骤 | 内容 | 优先级 |
|------|------|--------|
| S2-A | CPU 固定降频 160→80MHz (sdkconfig) | 1 |
| S2-B | BLE 广播间隔 30-50ms → 100-200ms | 2 |
| S2-C | Auto Light Sleep + FreeRTOS Tickless + UART 唤醒阈值 | 3 |
| S2-D | GPIO13 轮询改中断 + RTC GPIO 唤醒 | 4 |

### 关键决策
- 固定降频到 80MHz（不做动态调频）
- Light Sleep 只在空闲态生效，不影响 BLE 连接/广播
- UART 唤醒阈值设 3 字节（防噪声）
- GPIO13 从轮询改为下降沿中断，阻塞等待任务通知，零空闲唤醒

---

## 2026-07-17: S3 BLE 自动重连 — 已规划

- 步骤编号: S3 (S3-A + S3-B)
- 讨论ID: 20260717-ble-auto-reconnect
- 目标: 配对成功后，开关机手柄自动回连，无需按GPIO13

### 根因
`start_advertising()` 第894行检查 `pairing_window_active()`。连接成功后窗口被关闭，断连后广播无法重启。

### 子步骤

| 步骤 | 内容 | 优先级 |
|------|------|--------|
| S3-A | 广播与配对窗口解耦：移除start_advertising中的pairing_window条件，删除窗口过期停广播代码 | 1 |
| S3-B | 全时段广播保活：保活逻辑不限于配对窗口 | 2 |

### 实现摘要 (2026-07-17)

**修改文件**: `main/main.c` (3处改动)

1. `start_advertising()` (L894): 移除 `|| !pairing_window_active()` 条件 — 广播不再依赖配对窗口
2. `pair_button_task` (L463-469): 删除配对窗口过期停止广播的代码块
3. `pair_button_task` (L455): 广播保活条件移除 `pairing_window_active()` 限制 → 全时段保活

**编译**: ✅ 通过，0 错误 0 警告

### 关键决策
- 改动3处，均在 main/main.c
- pairing_window 只控制清 bond / 允许新设备配对
- 广播不再依赖配对窗口
- 安全配置不变（bonding + SC 已开启）

---

## 2026-07-20: S3-C BLE 重连修复 — 已完成

- 步骤编号: S3-C
- 讨论ID: (无，在计划模式下完成)
- 功能: 修复电脑蓝牙开关后无法自动重连

### 实现摘要

**修改文件**: `main/main.c` (4处改动)

1. 新增 `RECONNECT_ADV_DELAY_MS 1500` + `s_disconnect_tick` 变量
2. `BLE_GAP_EVENT_DISCONNECT` 移除 `start_advertising()` —— gap_event 运行在 NimBLE Host 任务上下文，直接调用 ble_gap_adv_start() 不可靠（NimBLE 文档警告）
3. `pair_button_task` 保活增加 1.5s 延迟检查 —— 产生"消失间隙"重置主机自动连接状态机
4. `BLE_GAP_EVENT_ENC_CHANGE` 增加加密失败处理 —— 清理 bond + 断开 → 下次走 REPEAT_PAIRING

### 关键决策
- 广播重启从 gap_event(context=NimBLE Host) 移到 pair_button_task(context=FreeRTOS task)
- 1.5s 延迟 = 给电脑蓝牙栈稳定时间 + 产生广播间隙

---

## 2026-07-20: S4 定向广播 + 断连空闲降速 — 已规划

- 步骤编号: S4 (S4-A ~ S4-D)
- 讨论ID: 20260720-directed-adv-sleep
- 功能: 断连+按键→定向广播快速重连；断连1min无重连→200ms慢广播省电

### 子步骤

| 步骤 | 内容 | 优先级 |
|------|------|--------|
| S4-A | 保存配对主机地址 + 定向广播基础结构 | 1 |
| S4-B | 断连1分钟降速到200ms慢广播 | 2 |
| S4-C | UART按键活动触发定向广播burst | 3 |
| S4-D | 状态机保护 + 边界条件 + 编译验证 | 4 |

### 关键决策
- 定向广播用 BLE_GAP_CONN_MODE_DIR + high_duty_cycle=1 + 1.28s burst
- 慢广播固定 200ms（用户确认）
- 1分钟计时从断连时刻开始（复用 s_disconnect_tick）
- 每次按键都重启 1.28s 定向 burst
- 首次配对前无 peer addr → 跳过定向，用普通广播

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

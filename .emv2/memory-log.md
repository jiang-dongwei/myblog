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

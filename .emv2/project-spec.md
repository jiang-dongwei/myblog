# 项目规格单

## 项目信息

| 字段 | 值 |
|------|-----|
| 项目名称 | ESP32-C6 BLE HID Gamepad Test (Fightpad 12 Slim) |
| 芯片 | ESP32-C6 (RISC-V, 160MHz) |
| SDK | ESP-IDF v5.5.1 |
| 平台 | esp32c6 |
| 构建目标 | fightpad12slim_c6_ble_hid |
| Flash | 4 MB |
| 主要文件 | main/main.c (~1025行) |

## 项目概述

ESP32-C6 独立 BLE HID 固件。从 UART0 (GPIO16/GPIO17) 接收外部输入状态(RP2350发送)，将其转换为标准蓝牙游戏手柄报告发送给主机。

## 现有功能

1. UART0 二进制帧解析 (8字节帧: F+Type+Payload+Checksum)
2. BLE HID Gamepad 报告 (16按钮+Hat+X/Y轴)
3. BLE 传输模式控制 (蓝牙/USB切换)
4. BLE Battery Service 电量上报
5. GPIO13 配对按键 (60秒配对窗口)
6. 启动完成通知 (C6_DONE\n 发送20次)

## 开发步骤状态

| 步骤 | 内容 | 状态 |
|------|------|------|
| S1-A | FW_INFO帧协议：多帧分包、flag/seq、CPU架构映射 | ✅ 已完成 |
| S1-B | 固件信息采集+payload构建+app_main()集成+CMake宏 | ✅ 已完成 |
| S1 | 读取固件信息(SDK/Plat/Board/CPU)，通过UART发送给RP2350 | ✅ 已完成 |
| S2-A | CPU固定降频 160→80MHz (sdkconfig) | ✅ 已完成 |
| S2-B | BLE广播间隔 30-50ms → 100-200ms | ✅ 已完成 |
| S2-C | Auto Light Sleep + UART唤醒阈值 | ✅ 已完成 |
| S2-D | GPIO13轮询改中断 + RTC唤醒 | ✅ 已完成 |
| S3-A | 广播与配对窗口解耦：移除pairing_window条件，删除窗口过期停广播 | ✅ 已完成 |
| S3-B | 全时段广播保活：保活逻辑不限于配对窗口 | ✅ 已完成 |
| S3-C | BLE重连修复：断连延迟1.5s+加密失败处理+NimBLE上下文合规 | ✅ 已完成 |
| S4-A | 保存配对主机地址 + 定向广播基础结构 (directed adv) | ⏳ 待开发 |
| S4-B | 断连1分钟降速到200ms慢广播 | ⏳ 待开发 |
| S4-C | UART按键活动触发定向广播burst | ⏳ 待开发 |
| S4-D | 状态机保护 + 边界条件 + 编译验证 | ⏳ 待开发 |

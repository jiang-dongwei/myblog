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
5. GPIO13 配对按键 (30秒配对窗口)
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
| S5-A | Profile v1 Mode/ACK UART协议与固定测试向量 | ✅ 已完成 |
| S5-B | NVS Profile/pending 与启动同步状态机 | ✅ 已完成 |
| S5-C | Generic Profile 模块化与逐字节回归 | ✅ 已完成 |
| S5-D | Xbox Layout HOGP描述符和USB XInput语义编码器 | 🔄 验证中 |
| S5-E | Profile切换、bond清理、30秒配对与绑定设备自动重连 | 🔄 验证中 |
| S5-F | UART0 Console隔离、测试和ESP32-C6编译 | 🔄 验证中 |
| S6-A | Xbox Series X|S 1914 BLE PnP、序列号和Report Map对齐 | ✅ 已完成 |
| S6-B | 1914 Profile主机回归测试与ESP32-C6编译 | ✅ 已完成 |
| S6-C | Windows XInput驱动、网页识别、配对与重连实机验收 | ⏳ 待验证 |

## S6 Xbox Series X|S 1914 BLE Profile

- 讨论ID：`20260813-xbox-series-1914-ble`。
- 仅调整ESP32-C6 Xbox Profile，目标身份为`045E:0B13/0509`，并采用参考工程的
  1914序列号和逐字节HID Report Map。
- ESP32-C6仍使用原生BLE HOGP，不尝试芯片不支持的Classic Bluetooth/BR/EDR。
- 16字节Input Report 1继续复用RP2350 USB XInput语义；保留Output Report 3但
  不实现振动。
- 保留GPIO13配对、bond保护、定向重连、UART0和双模式持久化；普通开机不得清bond。
- Windows实机必须删除旧缓存设备并重新配对后，再判断是否加载BLE XInput驱动以及
  浏览器是否显示Xbox/`mapping: standard`。

## S5 Xbox Layout BLE HID

- 讨论ID：`20260813-xbox-layout-ble-hid`
- Xbox Profile ID `1`，由 RP2350 通过 UART0 Mode v1 帧选择。
- Xbox按键语义严格复用RP2350 USB XInput实现。
- 所有Profile蓝牙名称均为`FP12Slim-C6`；Xbox Profile使用
  ESP32-BLE-CompositeHID的Xbox One S 1708 PnP身份`045E:02FD/0408`和334字节
  HID布局，但实测Windows浏览器仍可能把标准BLE HOGP暴露为`Unknown`，不能承诺真正
  XInput/WGI身份或`mapping: standard`。
- Xbox主输入Report 1为16字节；A2、Turbo和右摇杆不导出，LT/RT为10位数字扳机轴。
  为匹配1708描述符保留8字节Output Report 3，但固件忽略输出且不实现振动。
- NimBLE每个HID服务的Report特征上限为4，以容纳1708描述符的Report 1/2/3/4。
- 全新/无效NVS默认选择Xbox，但不清bond、不自动配对；v1~v5状态迁移保留原Profile并
  清除迁移遗留pending。首次配对仍由GPIO13显式触发。
- Profile变化后仅重启C6、清旧bond并打开30秒配对；相同Profile保留bond。
- 已绑定设备断线后先尝试定向快速广播，再使用仅允许该bond连接的白名单普通广播。
- UART0 GPIO16/GPIO17专用于RP2350协议，ESP-IDF Console改到USB Serial/JTAG。
- RP2350的USB类型和BLE类型分别持久化；本轮只修改BLE protobuf有效位与默认初始化，
  不在本项目流程中编译 `E:\ComporyProject\aa\GP2040-CE`。

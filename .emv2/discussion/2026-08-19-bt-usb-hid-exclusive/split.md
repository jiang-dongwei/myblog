# 需求拆分: 蓝牙档位 USB HID 互斥

讨论ID: `2026-08-19-bt-usb-hid-exclusive`

## 1. USB 设备连接状态机

- 类型: 通信 / 状态机
- 简述: GP33 选择蓝牙时让 RP2350 的 TinyUSB Device 从 USB 总线断开，避免主机同时枚举一个无输入的 USB 手柄。

## 2. 传输切换安全

- 类型: 控制
- 简述: USB 切到蓝牙时先中和旧 USB 输入，再断开；蓝牙切回 USB 时重新连接并枚举。

## 3. 特殊启动模式兼容

- 类型: 兼容性
- 简述: Web Config 必须始终保持 USB 连接；BOOTSEL 继续由 ROM 启动路径处理。


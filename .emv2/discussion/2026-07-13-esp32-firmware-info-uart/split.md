# 需求拆分: ESP32-C6 固件信息串口接收与菜单显示

讨论ID: `2026-07-13-esp32-firmware-info-uart`

## 1. UART 接收接入

- 类型: 通信
- 简述: 复用 `FightpadESP32ProxyAddon` 的 UART0，在关闭 USB CDC 时也持续读取 GP45 上的 ESP32-C6 UART0 TX 数据。

## 2. 固件信息帧状态机

- 类型: 通信 / 状态机
- 简述: 从 Console 日志与二进制帧混合字节流中同步 `0x46 0x49` 固件信息帧，并完成 XOR、flag、seq、超时和边界检查。

## 3. Payload 解析与信息缓存

- 类型: 数据管理
- 简述: 重组 `key=value\n` Payload，解析 SDK、Plat、Board、CPU，并以跨核安全方式从 Core0 发布给 Core1。

## 4. ESP32 固件菜单页面

- 类型: 显示
- 简述: 替换现有 ESP32 INFO 占位页；有数据时显示四项固件信息，无数据时显示 `Coming to soon`。

## 5. 验证

- 类型: 测试
- 简述: 进行非构建静态检查，随后由用户编译、烧录并完成实机上电验证。

## 用户确认

用户输入 `继续`，确认需求拆分通过。

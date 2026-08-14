# 需求拆分：Xbox Layout BLE HID

讨论 ID：`20260813-xbox-layout-ble-hid`

## 1. Profile 控制

- 类型：UART 通信 / 状态机
- RP2350 继续作为 BLE Profile 真值来源。
- ESP32-C6 解析 v1 `FM` Mode 帧并回复 `FA` ACK。
- 只在 Xbox Profile（协议编号 `1`）下启用 Xbox Layout。

## 2. Xbox Layout HID

- 类型：BLE HID
- 使用标准 HOGP Game Pad 描述符表达 Xbox 风格布局。
- 不实现或宣称 Microsoft XInputHID、XUSB 或 GIP。
- 不冒用 Microsoft VID/PID、制造商或产品身份。

## 3. 输入编码

- 类型：数据转换
- RP2350 的现有 `FP` 8 字节输入帧保持不变。
- 按键语义严格复用 RP2350 USB XInput 驱动映射。
- 数字 L2/R2 编码为 `0x00/0xFF` 扳机轴；无振动输出。

## 4. Profile 持久化与启动选择

- 类型：存储 / 启动状态机
- NVS 保存活动 Profile 和断电安全 pending 标志。
- HID 初始化前提供约 500 ms 同步窗口。
- 运行时切换先 ACK，再仅重启 ESP32-C6。

## 5. Windows 识别与缓存

- 类型：兼容性
- Xbox 模式名称为 `Fightpad Xbox Layout`。
- Profile 变化时清理旧 bond 并打开 30 秒配对窗口。
- 首次验证由用户在 Windows 删除旧设备后重新配对。
- Profile 独立 MAC 与长期重连留给 `Fix-MAC-connection` 分支。

## 6. 验证

- 类型：测试
- 固定 UART 向量、输入映射、NVS 状态和 HID 报告单元测试。
- ESP32-C6 静态检查和完整编译。
- Windows `joy.cpl`、Steam/SDL 识别由实机验收确认。

# BLE Controller Profiles - 需求拆分

讨论ID：`2026-08-12-ble-controller-profiles`

## 1. 蓝牙模式选择

- 类型：状态机 / 显示
- 简述：在 Fightpad 菜单中提供 `XBOX BLE`、`GENERIC BLE`、`KEYBOARD BLE`、`PS5 BLE (PC)` 四种蓝牙 Profile。

## 2. RP2350 到 ESP32-C6 模式通信

- 类型：UART 通信
- 简述：在现有 8 字节 XOR 帧协议上新增 BLE Profile 模式帧，不改变现有按键、传输、电池和状态帧。

## 3. ESP32-C6 多套 HID Profile

- 类型：蓝牙通信 / 状态机 / 存储
- 简述：ESP32-C6 根据已选模式，在 HID 初始化前选择设备身份、报告描述符和报告编码，并持久化当前模式。

## 4. PS5 基础报告映射

- 类型：输入映射
- 简述：以 PC、Steam 和 SDL 识别为目标，实现 DualSense 类基础按键报告；第一版不实现触摸板、陀螺仪、加速度计和自适应扳机。

## 5. 配对与主机缓存处理

- 类型：连接状态机 / 存储
- 简述：处理 Profile 变化后的 BLE 重启、Windows GATT/HID 缓存、绑定信息以及重新配对行为。

## 范围边界

- PS5 Profile 定位为 `PS5 BLE (PC Experimental)`。
- 不承诺连接真实 PS5 主机。
- 未经授权不把第三方 VID/PID 伪装方案直接定义为量产合规方案。


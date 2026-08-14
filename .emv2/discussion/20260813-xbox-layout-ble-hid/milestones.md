# 子流程拆分：S5 Xbox Layout BLE HID

## S5-A：Profile v1 UART 协议

- 新增 Mode/ACK 常量、解析器和固定向量测试。
- 保留现有 P/T/B/I/S 帧行为。
- 验证错误 checksum、错误 version、越界 Profile 和 sequence 回显。
- 优先级：1。

## S5-B：NVS Profile 与启动状态机

- 保存 active Profile、clear-bonds pending、pair-after-boot。
- HID 初始化前提供约 500 ms 同步窗口。
- 运行时 ACK 完整发送后延迟重启 C6。
- 优先级：2，依赖 S5-A。

## S5-C：Generic 基线模块化

- 将当前 Generic 描述符和 5 字节编码器迁入独立 Profile 模块。
- 逐字节测试确保当前 Generic 行为无回归。
- 优先级：3，依赖 S5-B。

## S5-D：Xbox Layout 描述符与编码器

- 实现 `Fightpad Xbox Layout` HOGP 描述符。
- 严格复用 RP2350 USB XInput 按键语义。
- 数字 LT/RT、左摇杆、固定中值右摇杆；不支持振动。
- 增加中性、单键、组合键、方向、扳机和轴方向测试向量。
- 优先级：4，依赖 S5-C。

## S5-E：Profile 切换、bond 与配对

- Profile 未变时保留 bond 且不重启。
- Profile 改变时清旧 bond、选择新描述符并开启 30 秒配对。
- 保留 GPIO13 和普通启动行为，不引入每次开机自动配对。
- 优先级：5，依赖 S5-D。

## S5-F：UART0 隔离与完整验证

- ESP-IDF Console 主输出切到 USB Serial/JTAG，UART0 GPIO16/GPIO17 专用于协议。
- 主机单元测试、`git diff --check`、ESP32-C6 完整编译。
- 生成 BIN 路径、大小和 SHA-256，并列出 Windows 实机测试项。
- 优先级：6，依赖 S5-E。

## 人机验证

- 用户烧录后删除 Windows 旧设备并重新配对。
- 验证名称、A/B/X/Y、View/Menu、D-pad、LB/RB、LT/RT 和左摇杆方向。
- 验证相同 Profile 重选不重启，普通开机不自动重新配对。
- `Fix-MAC-connection` 分支另行验证 Profile 地址/bond 隔离与长期重连。

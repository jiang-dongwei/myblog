# 技术方案：Xbox Layout BLE HID

## 1. Profile 专用描述符还是复合描述符

- 方案 A：启动时按 Profile 选择单一描述符，切换后重启 C6。
- 方案 B：始终暴露包含多个控制器的复合描述符。
- 决策：采用方案 A。Windows 只枚举当前 Profile，产品类型更清晰。

## 2. Xbox 报告结构

- 使用标准 HOGP Game Pad application collection。
- 按钮顺序复用 RP2350 USB XInput 语义。
- LT/RT 使用 8 位轴表达数字扳机。
- LX/LY 使用 UART X/Y，并保证主机可见方向与 USB 模式一致。
- RX/RY 固定中值。
- 不提供振动 Output Report。

具体字节布局在实现阶段固化为独立结构，并以逐字节单元测试保护。

## 3. Windows GATT/HID 缓存

- 当前分支采用 Profile 变化时清 bond、打开 30 秒配对窗口的方法。
- 实机首次测试要求 Windows 删除旧 `FP12Slim-C6` 后重新配对。
- Profile 独立 BLE 地址、bond 分区和长期自动重连由
  `Fix-MAC-connection` 分支处理，避免本分支混入另一问题域。

## 4. UART 与 Console 冲突

- 板间协议必须保留 UART0 GPIO16/GPIO17。
- 将 ESP-IDF Console 主输出切到 USB Serial/JTAG，防止日志字节混入二进制协议。
- ACK 发送成功需要检查完整写入并等待 TX 完成，然后才能重启。

## 5. 断电安全

- 使用版本化 NVS 状态保存 active Profile、clear-bonds pending 和 pair-after-boot。
- ACK 与重启之间掉电时，下次启动仍执行 pending 操作。
- 无效 Profile 回退 Generic；错误版本不修改现有状态。

## 6. 实现边界

- ESP-IDF 提供 BLE/NimBLE/HOGP 基础设施，项目实现 Profile、描述符和编码器。
- 不实现 XInputHID、XUSB、GIP、认证或微软身份。
- 不修改或构建 RP2350 工程。

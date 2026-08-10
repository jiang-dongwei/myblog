# 需求拆分

## 1. 普通 Chase 速度调整

- 类型：灯光控制
- 简述：将统一 Light Effect 的普通 Chase 从 160ms/格调整为 100ms/格，GP22/GP40继续共享动画步进，蓝牙状态 Chase 保持50ms/格。

## 2. Controller Type 菜单

- 类型：人机交互
- 简述：在现有拨轮主菜单增加 Controller Type 子菜单，提供 XBOX、PS3、PS4、PS5、SWITCH、SWITCH PRO、KEYBOARD、GENERIC HID 八项并标记当前模式。

## 3. Input Mode 持久化与重启

- 类型：状态管理 / 存储
- 简述：复用上游 GamepadOptions.inputMode 与强制保存后重启路径，使新USB描述符和驱动在重启后生效。

## 4. 模式能力边界

- 类型：兼容性
- 简述：XBOX映射为PC常用XInput，不开放当前认证不完整的Xbox One；PS5继续受现有USB认证配置约束；本功能默认只改变RP2350 USB输入模式。

## 已确认原则

- Controller Type 项目为 XBOX / PS3 / PS4 / PS5 / SWITCH / SWITCH PRO / KEYBOARD / GENERIC HID。
- 不增加独立 Arcade 项；上游没有独立 Arcade 输入模式，Arcade Stick 属于设备类型而不是 USB 输入模式。
- 模式切换沿用上游输入模式配置，不实现自定义USB协议；PS4、PS5继续使用现有认证机制。
- 普通 Chase 改为100ms/格，蓝牙状态 Chase不变。

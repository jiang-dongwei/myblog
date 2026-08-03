# 技术方案: 菜单期间锁定游戏交互

讨论ID: `2026-07-21-menu-game-input-lockout`

## 已否决方案

1. **只在菜单插件清空 GamepadState**：会遗漏后续插件、GP20 原始 GPIO 和传输差异。
2. **只在 ESP32-C6 锁定**：只能控制 BLE HID，无法控制 RP2350 USB、Turbo、宏和热键。
3. **使用 FP buttons bit15 传锁状态**：旧 ESP32 会把它透传为 HID Button16。
4. **新增独立菜单帧**：需要额外帧同步、周期重发与恢复逻辑。

## 采用方案

- RP2350 菜单模块维护 `UNLOCKED/CAPTURED/DRAIN_UNTIL_RELEASE`。
- RP2350 USB 驱动调用窗口临时使用 neutral 状态并屏蔽 GP2～GP20，调用后恢复真实状态。
- `processedGamepad` 保留真实快照，供 ESP32 收到原始输入并自行执行 BLE 锁定。
- `FP` Byte4 bit7 表示 `MENU_GAMEPLAY_LOCK`；Byte4 bit0～3 仍为 D-pad。
- Byte4 高位被旧 ESP32 安全忽略，不会产生虚假 HID Button16。
- ESP32 采用 `UNLOCKED/LOCKED/DRAIN_UNTIL_RELEASE`，锁定时向 BLE 主机发送 neutral。

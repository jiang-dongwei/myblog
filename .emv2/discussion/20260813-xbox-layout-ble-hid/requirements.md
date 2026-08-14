# 需求确认：Xbox Layout BLE HID

## 产品目标

- Xbox Profile 下，PC 显示 `Fightpad Xbox Layout`，并按 Xbox 风格解释按键。
- 目标是标准 BLE HID Game Pad 与 Windows/Steam/SDL 兼容，不要求真正的
  Microsoft Xbox/XInput 设备身份。
- Generic、Keyboard、PlayStation Layout 不应被 Xbox 编码器覆盖。

## 身份

| 字段 | Xbox Layout |
|---|---|
| Name | `Fightpad Xbox Layout` |
| Manufacturer | `Fightpad` |
| VID:PID | `1209:2040` |
| Version | `0002` |
| Appearance | Gamepad |

不得使用 Microsoft 名称、VID/PID 或认证字段。

## 权威输入映射

映射依据为 RP2350 USB 模式的 `XInputDriver::process()`，而不是重新猜测按钮顺序。

| RP2350 输入 | Xbox Layout 输出 |
|---|---|
| B1/B2/B3/B4 | A/B/X/Y |
| L1/R1 | LB/RB |
| L2/R2 | LT/RT，释放 `0x00`，按下 `0xFF` |
| L3/R3 | Left/Right stick click |
| S1/S2 | Back(View)/Start(Menu) |
| A1 | Home/Guide |
| A2 | 不导出，与当前 USB XInput 行为一致 |
| D-pad | 上/下/左/右 |
| UART X/Y | 左摇杆，主机可见方向与 USB XInput 一致 |
| 右摇杆 | 固定中值，UART 当前未提供 RX/RY |
| Turbo bit14 | 不导出，与当前 USB XInput 行为一致 |

## 输出功能

- 第一版不支持振动。
- 不创建 rumble Output Report。
- 不向 RP2350 增加振动回传协议。

## 验收

- Xbox Profile 下 Windows 显示 `Fightpad Xbox Layout`。
- `joy.cpl`、Steam 或 SDL 能枚举并读取控制器。
- A/B/X/Y、View/Menu、D-pad、LB/RB、LT/RT 与 USB XInput 布局一致。
- A2、Turbo、右摇杆和振动不会产生错误输入。
- 相同 Profile 不重启、不清 bond。
- 不修改 `E:\ComporyProject\aa\GP2040-CE`。

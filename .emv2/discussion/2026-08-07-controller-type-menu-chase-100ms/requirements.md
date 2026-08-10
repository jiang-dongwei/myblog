# 详细需求

## 普通 Chase

- 将统一 Light Effect 的普通 Chase 步进时间由 160ms 改为 100ms。
- GP22 按键灯链与 GP40 底部灯链继续共用同一个 Chase 步进状态。
- 蓝牙连接状态使用的独立 Chase 保持 50ms，不受本次修改影响。

## Controller Type 菜单

- 在拨轮主菜单中增加 `Controller Type`，位置放在 `RGB Customize` 之后。
- 子菜单固定提供 `XBOX`、`PS3`、`PS4`、`PS5`、`SWITCH`、`SWITCH PRO`、`KEYBOARD`、`GENERIC HID`。
- `XBOX` 对应 `INPUT_MODE_XINPUT`，不提供 Xbox One 模式。
- `KEYBOARD` 对应 `INPUT_MODE_KEYBOARD`；`GENERIC HID` 对应 `INPUT_MODE_GENERIC`，作为上游通用 HID 模式，不另行命名为 DInput。
- 不提供独立 `Arcade` 输入模式；本菜单只移植上游已有的 `InputMode`。
- 当前已保存的模式在名称后显示 `*`。
- 选择当前模式时不保存、不重启。
- 选择不同模式时更新 `GamepadOptions.inputMode`，强制写入 Flash，并在保存完成后重启。
- 重启后由现有驱动选择流程加载对应 USB 手柄驱动和描述符。

## 能力边界

- 本菜单只修改 RP2350 的有线 USB 输入模式，不修改 ESP32-C6 蓝牙 HID 模式。
- PS4、PS5 模式继续受项目现有 USB 认证配置约束。
- 不新增 USB 协议、不修改现有启动按键切换逻辑。

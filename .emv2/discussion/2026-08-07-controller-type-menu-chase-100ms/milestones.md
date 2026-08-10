# 实施里程碑

## S31-A Controller Type 菜单与模式映射（completed）

- 在主菜单 `RGB Customize` 后增加入口。
- 增加八个上游模式并用 `*` 标记当前模式。
- 进入子菜单时定位到当前模式；BACK 返回主菜单原入口。

## S31-B 保存、重启与边界（completed）

- 相同模式不保存、不重启。
- 不同模式更新 `GamepadOptions.inputMode`，触发 `GPStorageSaveEvent(true, true)`。
- 不新增 Arcade、DInput 私有协议，不修改 ESP32-C6 蓝牙 HID。

## S31-C 静态验证（completed）

- 检查八项与上游枚举的一一映射。
- 检查显示、导航、保存和重启路径。
- 检查现有并行改动未被覆盖，并执行 `git diff --check`。

## S31-D 构建烧录与实机验证（pending）

- 由用户构建烧录。
- 逐项确认 USB 重新枚举、当前项星号、相同项无重启，以及 PS4/PS5 认证边界。

## S32-A 普通 Chase 100ms（completed）

- 普通 Light Effect 的公共 Chase 步进由 160ms 调整为 100ms。
- GP22/GP40 继续共享同一动画步进；蓝牙状态 Chase 保持 50ms。

## S32-B 普通与蓝牙 Chase 实机验证（pending）

- 由用户构建烧录，确认普通 Chase 加速且两条灯链同步。
- 确认 Pairing/Connecting 蓝牙 Chase 速度和优先级未改变。

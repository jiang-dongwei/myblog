# 方案确认: RP2350B 固件信息菜单

讨论ID: `2026-07-10-rp2350-firmware-info-menu`

## 方案A: 渲染时读取编译期宏

- 在 `DisplayAddon::drawScrollWheelMenu()` 中直接使用 `pico/version.h` 和目标构建宏。
- 不增加跨核共享状态，不写入 Flash。
- 每次编译自动携带对应版本信息。

## 方案B: Core0 复制到菜单状态

- 扩大 `ScrollWheelMenuState` 并复制字符串给 Core1。
- 会增加不必要的跨核同步和字符串生命周期管理。

## 决策

采用方案A。使用固定 22 字节缓冲区和 `snprintf`，确保 21 字符显示宽度及结尾空字符。

## 用户确认

用户输入 `继续`，确认采用方案A。

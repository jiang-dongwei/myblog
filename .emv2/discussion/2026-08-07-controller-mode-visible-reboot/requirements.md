# 详细需求

- 仅选择不同 Controller Type 时触发重启黑屏；选择当前模式不触发。
- 在现有约 500ms 保存重启延时内，让 GP22 与 GP40 至少写入一帧全黑。
- 不修改 `manualLightEffectsEnabled`、颜色、效果或亮度的持久化值。
- 重启黑屏优先于普通灯效和蓝牙临时灯效。
- 不改变现有 watchdog 硬件重启和 USB 重新枚举路径。

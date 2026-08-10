# 方案推演

## 采用：RAM 重启黑屏标志

Controller Type 发生变化时置位一次性 RAM 标志。灯光插件在 `render()` 和 `show()` 最终输出层同时检查该标志，清空两条灯链并屏蔽蓝牙覆盖。watchdog 重启后 RAM 重新初始化，原 Flash 灯效自动恢复。

## 未采用

- 不把 `g_menuRgbPowerEnabled` 或 GP30 状态保存为 OFF，否则重启后灯效不会恢复。
- 不直接从菜单代码写 GP24，继续保持灯光插件为唯一电源写入者。
- 不全局启用 GP24 OFF 门控，避免扩大到普通 All OFF、低电和蓝牙状态路径。

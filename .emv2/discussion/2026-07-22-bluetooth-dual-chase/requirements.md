# 需求确认

讨论ID：`2026-07-22-bluetooth-dual-chase`

## 编译期效果选择

- 新增宏：`FIGHTPAD12SLIM_ESP32_BT_STATUS_DUAL_CHASE`
- 宏值 `1`：使用新版双段 Chase。
- 宏值 `0`：使用旧版单段 5 灯 Chase。
- Fightpad12Slim 的 `BoardConfig.h` 当前默认设置为 `1`。
- 公共头文件提供默认值 `0`，避免其他板级配置意外启用 Fightpad12Slim 专用效果。

## 新版双段 Chase

- 适用状态：`Pairing`、`Connecting`。
- 输出灯链：GP40 Base，19 颗 WS2812。
- 颜色：纯蓝色。
- 每段灯数：3 颗。
- 每段亮度：头灯 `80%`，第一颗尾灯 `25%`，第二颗尾灯 `5%`。
- 尾灯位于运动方向之后。
- 第二段头灯相对第一段偏移 9 个逻辑灯位。
- 两段同方向、同速度移动。
- 速度：每 `50 ms` 前进 1 个逻辑灯位。

## 兼容与隔离

- 旧版分支完整保留：5 灯纯蓝梯度 `5%/25%/80%/25%/5%`，速度同样保持 `50 ms/格`。
- `Connected` 继续显示临时全蓝。
- `Disconnected` 继续显示临时全黑。
- GP22 按键灯不参与蓝牙状态 Chase。
- 普通菜单 Base/Key Chase 不受编译宏影响。
- BQ27220 `SOC <= 7%` 低电关灯优先级不变。
- All OFF 状态下临时唤醒 GP40 的既有行为不变。


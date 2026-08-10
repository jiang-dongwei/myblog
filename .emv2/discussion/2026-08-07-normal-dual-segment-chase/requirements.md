# 详细需求

- GP40 19 灯链：两个 Chase 头灯相隔 `count / 2`，即 9 格。
- GP22 12 灯链：两个 Chase 头灯相隔 `count / 2`，即 6 格。
- 每段亮度依次为 `0.80f`、`0.25f`、`0.05f`，方向与蓝牙 Pairing/Connecting 双段 Chase 一致。
- 普通 Chase 保留动态色轮颜色，不改为蓝牙状态的纯蓝。
- GP22 Key Flash 继续覆盖对应灯位。
- 普通速度保持 100ms/格，蓝牙状态速度保持 50ms/格。

# 需求确认

讨论ID：`2026-08-07-unified-light-effect-chase-speed`

## 菜单结构

- `RGB Customize` 调整为 `Key Flash`、`Light Effect`、`Brightness`、`All OFF`。
- `Light Effect` 统一提供 `Static Color`、`Gradient`、`Breathing`、`Rainbow`、`Chase`。
- `Static Color` 与 `Breathing` 共用一个颜色选择入口，选择后同时写入 GP22 和 GP40 的基础颜色。
- OLED 当前效果与当前颜色继续使用 `*` 标记。

## 同步规则

- GP22 与 GP40 始终读取同一个运行时效果编号。
- Gradient、Rainbow 和 Chase 的动画状态每帧只更新一次，两条灯链读取同一相位。
- Chase 两条灯链使用同一动态色轮颜色和同一前进时刻。
- GP40 Chase 保留 5 灯 `5%/25%/80%/25%/5%` 对称梯度。
- GP22 Chase 保留 3 灯 `60%/25%/5%` 拖尾以及 Key Flash 覆盖。
- Rainbow 按各自灯珠数量均匀铺满色轮，因此相位同步但像素位置不要求一一对应。

## 速度

- 普通 Light Effect 的 Chase 默认步进间隔为 `160 ms`。
- 速度通过 `FIGHTPAD12SLIM_LIGHT_CHASE_STEP_MS` 提供公共默认值。
- 蓝牙状态临时 Chase 继续保持 `50 ms/格`，不受该宏影响。

## 旧配置兼容

- 保留 `buttonEffectIndex=4` 和 `ambientEffectIndex=5` 的 protobuf 字段与编号。
- 启动时将旧 Key/Base 效果编号转换为统一效果；两者不一致时优先采用 Key，Key 无有效值时采用 Base。
- 旧隐藏效果映射到最接近的现有统一效果。
- 统一效果保存时分别映射回旧 Key/Base 编号，旧固件仍能读取合法值。
- 旧颜色不一致时优先采用 Key 颜色；Key 未设置时采用 Base 颜色。
- `All OFF` 继续以三路黑色、两个效果字段 `0xFF` 表示，不增加 Flash 字段。

## 不变行为

- Key Flash 仍仅覆盖被按下的 GP22 灯珠。
- `SOC <= 7%` 低电关灯继续拥有最高优先级。
- 蓝牙状态只临时覆盖 GP40，结束后恢复统一效果。
- GP24 上下电与最终黑帧逻辑不变。
- Brightness 对 Static Color、Gradient、Rainbow 的现有档位不变。

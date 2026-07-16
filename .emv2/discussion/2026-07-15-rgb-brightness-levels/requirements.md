# 需求确认

讨论ID：`2026-07-15-rgb-brightness-levels`

## 菜单与交互

- `RGB Customize` 菜单顺序调整为：`Key Flash`、`Key Effect`、`Base Effect`、`Brightness`、`All OFF`。
- `Brightness` 下一级依次显示 `Bright`、`Normal`、`Dim`。
- 档位对应值为 `0.5f`、`0.3f`、`0.1f`。
- 当前生效档位在 OLED 列表右侧使用 `*` 标记。
- 短按 GP30 立即应用并持久化，选择后停留在亮度列表，便于实时比较。
- GP19 从亮度列表返回 `RGB Customize`。

## 共享状态与持久化

- Key Effect 与 Base Effect 共用一个亮度档位。
- 未配置或旧配置没有亮度字段时默认使用 `Bright (0.5f)`，保持旧固件视觉效果。
- 档位存入现有 `FightpadAmbientLEDOptions`，重启后恢复。
- 在不受控效果中切换档位仍然保存；以后进入受控效果时使用最近保存的亮度。

## 生效范围

- Key Effect：`Static Color`、`Rainbow`、`Gradient`。
- Base Effect：`Static Color`、`Gradient`、`Rainbow`。
- 不改变 `Key Flash`，其亮度继续使用 `0.8f`。
- 不改变 `Chase`、`Breathing`、`All OFF` 和 7% 低电强制关灯逻辑。
- 7% 低电强制关灯保持最高输出优先级。

## 用户确认

- 用户确认以上菜单位置、默认档位、交互方式、持久化方式和效果范围。


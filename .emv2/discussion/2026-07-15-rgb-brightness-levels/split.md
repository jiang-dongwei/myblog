# 需求拆分

讨论ID：`2026-07-15-rgb-brightness-levels`

## 1. 菜单与导航

- 在 `RGB Customize` 子菜单增加 `Brightness`。
- 进入亮度列表后提供三档：`Bright (0.5f)`、`Normal (0.3f)`、`Dim (0.1f)`。
- 不提供 `OFF` 档位。

## 2. 状态与持久化

- Key Effect 和 Base Effect 共用一个亮度档位。
- 亮度档位写入现有 Fightpad RGB 配置并在重启后恢复。
- 未设置时保持现有 `0.5f` 亮度。

## 3. 灯光渲染

- 亮度档位只控制 Key Effect 和 Base Effect 的 `Static Color`、`Gradient`、`Rainbow`。
- 不改变 `Key Flash`、`Chase`、`Breathing`、`All OFF` 和 7% 低电强制关灯逻辑。

## 4. 硬件范围

- 复用 GP22 的 12 颗按键灯和 GP40 的 19 颗环境灯。
- 不增加或修改引脚和外设。

## 用户确认

- 用户将原四档要求修改为三档，删除 `OFF`。
- 其余拆分内容确认正确。


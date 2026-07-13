# 需求确认: Key Effect Gradient

讨论ID: `2026-07-13-key-effect-gradient`

## 菜单行为

- 菜单路径: `RGB Custom -> Key Effect -> Gradient`。
- 菜单顺序: Static Color、Gradient、Breathing、Rainbow、Chase。
- Gradient 是终端效果，短按后立即生效并停留在 Key Effect 菜单。
- 当前效果标记继续复用现有 `targetIndex` 与 `g_menuButtonEffect` 比较逻辑。

## 视觉效果

- GP22 的 12 颗按键 LED 在同一帧显示相同颜色。
- 使用 `RGB::wheel()` 连续变色。
- 亮度固定为 `0.5f`。
- 色轮每帧步进 `2`，到达边界后反向。
- Gradient 忽略已保存的 Static Color，但不删除或覆盖该颜色。

## Key Flash

- 被按下按键的 Key Flash 颜色拥有显示优先级。
- Flash 结束后恢复当时的 Gradient 颜色。
- 其他按键继续显示 Gradient，不受单键 Flash 影响。

## 配置兼容

- Key Gradient 使用新效果编号 `6`。
- 保留已有编号 `0`、`1`、`2`、`4`、`5` 的含义。
- 选择结果通过现有 `buttonEffectIndex` 和 `GPStorageSaveEvent(false)` 持久化。

## 用户确认

用户输入 `继续`，确认以上需求规则。

# 头脑风暴: Key Effect Gradient

讨论ID: `2026-07-13-key-effect-gradient`

## 难点 1: 效果编号兼容

- 方案A: 复用 Key Rainbow 的编号 `1`，会导致旧配置语义改变。
- 方案B: 为 Gradient 分配新编号 `6`，保留已有编号含义。
- 决策: 采用方案B。

## 难点 2: Base 与 Key 动画状态耦合

- 方案A: 共用 `wheelFrame` 和 `wheelReverse`，Base 与 Key 同时启用动态效果时可能相互改变速度和方向。
- 方案B: Key Gradient 使用独立色轮位置和方向，算法参数保持与 Base Gradient 一致。
- 决策: 采用方案B。

## 难点 3: Key Flash 覆盖

- 方案A: Gradient 直接整帧填色，可能覆盖按键 Flash。
- 方案B: 每颗灯写入时先判断 `gp22FlashUntil`，Flash 有效时写入 Flash 颜色，否则写入 Gradient 颜色。
- 决策: 采用方案B。

## 难点 4: Static Color 保存

- 方案A: 选择 Gradient 时清空 `g_menuRgbTop`。
- 方案B: Gradient 渲染时忽略 Static Color，但保留其存储值，切回 Static Color 后恢复原选择。
- 决策: 采用方案B。

## 用户确认

用户输入 `继续`，确认全部采用方案B。

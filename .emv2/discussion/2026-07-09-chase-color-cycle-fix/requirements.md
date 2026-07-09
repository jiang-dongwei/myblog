# 需求确认: Chase 动态变色修复

讨论ID: `2026-07-09-chase-color-cycle-fix`

## 1. RGB Customize 菜单行为

**当前问题**: 选择 `Chase` 后，RGB 灯变成白色 Chase，颜色不再变化。

**预期行为**: 选择 `Chase` 后，效果保持 Chase 动画，并且颜色动态变化。

**不希望发生**:
- Chase 被固定成 White
- Chase 被 Static Color 的颜色覆盖
- 旧配置里保存的 White 把 Chase 固定成白色

## 2. Button LED / Ambient LED 范围

修复范围覆盖：

- `Button Effect -> Chase`: GP22 按键灯 Chase 应动态变色
- `Base/Ambient Effect -> Chase`: GP40 底部/环境灯 Chase 应动态变色

## 3. 颜色覆盖规则

建议规则：

- `Static Color` 才使用菜单选择的固定颜色
- `Chase` 使用自己的动态颜色序列
- `Rainbow`、`Breathing`、`Static Theme` 不被强制改成 White
- 旧配置里保存的 White 不应该把 Chase 固定成白色

## 4. 保存行为

保持现有菜单保存逻辑：

- 选择 Chase 后保存 effect index
- 不清空用户选择过的 Static Color
- 渲染 Chase 时忽略 Static Color 覆盖

## 用户确认

用户输入 `继续`，确认需求通过。

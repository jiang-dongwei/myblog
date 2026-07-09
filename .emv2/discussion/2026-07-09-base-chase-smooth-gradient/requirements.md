# 需求确认: Base Chase 平滑亮度梯度

讨论ID: `2026-07-09-base-chase-smooth-gradient`

## 1. 修改范围

**目标菜单**: `RGB Customize -> Base Effect -> Chase`

**目标灯链**: GP40 底部/环境 RGB 灯链

**追加修改**: `Button Effect -> Chase` / GP22 按键灯链改为前暗后亮。

## 2. Chase 视觉效果

**当前效果**: Chase 过程中连续亮起的灯亮度层次不够平滑。

**预期效果**: 追逐过程中首尾灯暗一些，中间灯亮一些，视觉上更像柔和的追逐光带。

建议亮度梯度：

`0.05 -> 0.25 -> 0.80 -> 0.25 -> 0.05`

也就是 5 颗连续灯参与 Chase，中间最亮，两侧逐渐变暗。

## 3. 颜色行为

沿用 S6 的结果：

- Chase 继续动态变色
- 不固定白色
- 不使用 Static Color override
- 只改变 Chase 灯带亮度分布，不改变颜色来源

## 5. GP22 Button Chase 追加需求

- `Button Effect -> Chase` 也改成前暗后亮
- 3 灯梯度使用 `0.05 -> 0.25 -> 0.80`
- 继续使用动态颜色

## 4. 保存/菜单行为

- 不新增菜单项
- 不新增配置项
- 选择 `Base Effect -> Chase` 的方式不变
- 只改变 Chase 的渲染观感

## 用户确认

用户输入 `继续`，确认需求通过。

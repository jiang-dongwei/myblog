# 头脑风暴: Chase 动态变色修复

讨论ID: `2026-07-09-chase-color-cycle-fix`

## 难点1: Chase 被颜色覆盖成白色

**分析**: 菜单里选择非 Static Color 效果时，代码可能为了“保证可见”，把黑色自动改成白色；但 Chase 本来应该自己动态变色，所以这个兜底会让 Chase 变成白色动画。

**方案选择**: A. 选择 Chase 时清空颜色覆盖

**用户确认**: 选择 A。

## 难点2: GP22 和 GP40 的效果逻辑可能不同

**分析**: Button LED 和 Ambient LED 可能各有一套 effect index 和 render 分支，修一边可能另一边还白。

**方案选择**: B. 同时检查并修 GP22 / GP40 两条路径

**用户确认**: 选择 B。

## 难点3: 旧配置兼容

**分析**: Flash 里可能已经保存了 `effect=Chase` + `color=White`。如果只改菜单选择逻辑，旧配置启动后仍可能白色。

**方案选择**: B. 渲染层按 effect 决定是否使用 color override

**用户确认**: 选择 B。

## 最终策略

- 新选择 Chase 时清空对应颜色覆盖，避免新配置继续保存 White/static color。
- 同时修 GP22 Button LED 和 GP40 Ambient LED 两条路径。
- 渲染层按 effect 类型决定颜色来源，兼容旧配置中已经保存的 White/static color。
- 不新增硬件、不改引脚。

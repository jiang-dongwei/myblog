# 子流程拆分: Base Chase 平滑亮度梯度

讨论ID: `2026-07-09-base-chase-smooth-gradient`

## S7-A: Base Chase 亮度梯度调整

- **所属子系统**: GP40 Base Effect 渲染
- **开发内容**:
  1. 修改 `renderAmbient()` 的 `AL_CUSTOM_EFFECT_CHASE` 分支
  2. Chase 光带从 4 颗改成 5 颗
  3. 梯度改为 `0.05, 0.25, 0.80, 0.25, 0.05`
  4. 每颗灯继续使用 `chaseColorFor()` 动态颜色
- **前置条件**: S6-B 已完成
- **验证方式**: 选择 `RGB Customize -> Base Effect -> Chase`，观察 GP40 追逐首尾暗、中间亮
- **优先级**: 1

## S7-B: 范围隔离确认

- **所属子系统**: GP22 Button Effect / 其他 RGB 模式
- **开发内容**:
  1. 将 GP22 Button Chase 改为前暗后亮
  2. 确认 Static Color 不受影响
  3. 确认 RGB OFF 不受影响
- **前置条件**: S7-A 完成
- **验证方式**: 切换 Button Chase，观察 3 灯梯度为前暗后亮；切换 Static Color、RGB OFF 做回归观察
- **优先级**: 2

## 用户确认

用户输入 `继续`，确认子流程拆分通过。

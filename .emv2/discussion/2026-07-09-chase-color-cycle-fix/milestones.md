# 子流程拆分: Chase 动态变色修复

讨论ID: `2026-07-09-chase-color-cycle-fix`

## S6-A: Chase 菜单选择时清空颜色覆盖

- **所属子系统**: ScrollWheelMenu 菜单选择逻辑
- **开发内容**:
  1. `Button Effect -> Chase` 被选中时，设置 `g_menuButtonEffect = Chase`
  2. 同时清空 GP22 对应的静态颜色覆盖，避免继续保存 White
  3. `Ambient Effect -> Chase` 被选中时，设置 `g_menuAmbientEffect = Chase`
  4. 同时清空 GP40 对应的静态颜色覆盖
- **前置条件**: 无
- **验证方式**: 菜单选择 Chase 后，保存配置并重启，Chase 不应固定白色
- **优先级**: 1

## S6-B: 渲染层按 effect 类型选择颜色源

- **所属子系统**: FightpadAmbientLEDAddon RGB 渲染
- **开发内容**:
  1. 检查 GP22 Button LED 渲染路径
  2. 检查 GP40 Ambient LED 渲染路径
  3. 当 effect 为 Chase 时，忽略静态颜色 override，使用动态颜色序列
  4. 保持 Static Color 继续使用用户选色
- **前置条件**: S6-A 完成
- **验证方式**: 旧配置里即使保存过 White，Chase 仍动态变色
- **优先级**: 2

## S6-C: 回归验证

- **所属子系统**: RGB Customize / OLED 菜单 / LED 输出
- **开发内容**:
  1. 验证 `Button Effect -> Static Color`
  2. 验证 `Button Effect -> Chase`
  3. 验证 `Base Effect -> Static Color`
  4. 验证 `Base Effect -> Chase`
  5. 确认 `RGB OFF` 不被破坏
- **前置条件**: S6-A、S6-B 完成
- **验证方式**: 编译通过 + 实机观察 GP22/GP40
- **优先级**: 3

## 用户确认

用户输入 `继续`，确认子流程拆分通过。

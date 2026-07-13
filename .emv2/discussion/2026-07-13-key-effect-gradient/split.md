# 需求拆分: Key Effect Gradient

讨论ID: `2026-07-13-key-effect-gradient`

## 1. Key Effect 菜单入口

- 类型: 显示 / 状态管理
- 简述: 在 RGB Custom 的 Key Effect 菜单中增加 `Gradient`，并保持已有菜单效果编号兼容。

## 2. GP22 Gradient 渲染

- 类型: 控制
- 简述: 为 GP22 的 12 颗按键灯增加与 Base Gradient 相同视觉规则的同步变色效果。

## 3. 配置持久化

- 类型: 存储
- 简述: 复用现有 `buttonEffectIndex` 保存 Key Gradient 选择，不覆盖已保存的 Static Color。

## 4. 验证

- 类型: 测试
- 简述: 进行非构建静态检查，随后由用户编译、烧录并完成菜单、灯效、Key Flash 和重启保持验证。

## 用户确认

用户输入 `继续`，确认需求拆分通过。

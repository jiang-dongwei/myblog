# 需求拆分: Chase 动态变色修复

讨论ID: `2026-07-09-chase-color-cycle-fix`

## 需求拆分

根据需求描述，分析出以下子系统/模块：

### 1. ScrollWheelMenu 菜单选择逻辑
- 类型：状态机 / 存储
- 简述：RGB Customize 里选择 Chase 时，会写入 `g_menuButtonEffect` 或 `g_menuAmbientEffect`，并持久化到配置。

### 2. FightpadAmbientLEDAddon RGB 渲染
- 类型：控制 / 显示
- 简述：实际驱动 GP22/GP40 RGB 灯链，决定 Chase 模式下使用固定白色还是动态变色。

### 3. Chase 效果颜色源
- 类型：显示 / 算法
- 简述：确认 Chase 应使用动态变化颜色，而不是被菜单颜色覆盖为 White。

### 4. 配置兼容
- 类型：存储
- 简述：避免旧配置里保存的白色/静态颜色覆盖 Chase，导致切换效果后仍然白色。

## 用户确认

- 拆分确认：对
- Chase 预期：固定为动态变色，不要固定白色
- RGB 渲染影响：对
- 配置兼容：对

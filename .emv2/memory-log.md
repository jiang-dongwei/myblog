# 记忆日志

## 2026-07-06: 拨轮开关菜单系统

### 决策记录

1. **GPIO30-32共享方案**: 使用B方案（独立GPAddon + 全局标志仲裁），而非扩展现有DisplayMode枚举
   - 原因: 避免侵入现有显示系统，保持DIP功能独立性
   - 仲裁方式: `volatile bool g_scrollWheelMenuActive`

2. **菜单架构**: 独立`ScrollWheelMenuAddon` (GPAddon)，直接调GPGFX渲染，不创建GPScreen子类
   - 原因: 导航方式完全不同（编码器 vs 手柄按键），不需要现有屏幕生命周期

3. **GPIO冲突解决**: 复用共存的方案 — 正常模式DIP生效，菜单模式DIP暂停，退出后恢复
   - `FightpadAmbientLEDAddon::process()` 加一行检查即可

### 关键约束

- 语言: 英文
- 不显示顶部标题栏
- 颜色控制功能暂不开发（仅显示色块列表）
- GP30长按3s进入菜单，长按3s退出（返回BUTTONS）
- 叶子节点短按GP30返回上一层（不到BUTTONS）
- RP2350B/ESP32C6信息页标注"Coming soon"

## 2026-07-07: 修复长按GP30菜单进入时RGB灯误切换

### 根因

`FightpadAmbientLEDAddon::readControls()` 中 `g_scrollWheelButtonBusy` 抑制 GPIO30 读取，
导致 `controls`(当前) 和 `lastControls`(上次) 不一致，`handleControlEdges()` 检测到假 release 边沿，
错误触发 `enabled = !enabled`。

### 修复

**第一轮**: 删除 `readControls()` 中对 `g_scrollWheelButtonBusy` 的 3 行抑制代码。
`handleControlEdges()` 已使用 release-edge 处理 ONOFF，`process()` 已有 `g_scrollWheelMenuActive` 检查，
两个机制组合已足够防止进入菜单时的长按 LED 误切换。

**第二轮 (exit bug)**: 新增 `g_scrollWheelButtonLongPressed` 全局标志。
`navToggle()` 在 3s 时将 `g_scrollWheelMenuActive = false`，但按钮仍在按下状态；
释放时 release edge 在 `handleControlEdges()` 中触发 LED 切换。
修复: 长按触发时设 `g_scrollWheelButtonLongPressed = true`，释放时清 false；
`handleControlEdges()` 检查此标志抑制 ONOFF release edge。

## 2026-07-07: 菜单导航行为优化 (S3-A)

### 修复 ①: INFO 页面禁用拨轮滚动
`process()` 中添加 `level != SWMenuLevel::INFO` 检查，INFO 页面不再响应 GP31/GP32 旋转。
INFO 是静态信息页，无菜单列表可滚动。

### 修复 ②: COLOR 层级短按返回上层
`navSelect()` 中对 `SWMenuLevel::COLOR` 做早退处理，直接返回 RGB_SUB。
COLOR 是终端列表层级，不应再能短按进入 INFO 页面（"删除层次3"）。
INFO 只能从 MAIN 层进入（RP2350/ESP32 信息页）。

## 2026-07-07: RGB 颜色控制 (S4-A, S4-B)

### 功能
通过菜单 COLOR 层级选色，覆盖环境灯和按键闪灯颜色。

### 架构
- `g_menuRgbTop` / `g_menuRgbBottom` / `g_menuRgbButton` — 全局颜色覆盖标志 (0xFF=未设置)
- `g_menuRgbTarget` — COLOR 层级中正在配置的目标 (0=Top, 1=Bottom, 2=Button)
- COLOR 短按: 写入 `g_menuRgbTarget` → 对应颜色变量 → 返回 RGB_SUB
- `render()`: 分别读取 top/bottom 覆盖 → 保持呼吸效果，仅替换色值
- `updateButtonFlash()`: 读取 button 覆盖 → 替换白色闪灯

### 讨论ID
`2026-07-07-rgb-color-control`

### 讨论ID
`2026-07-07-menu-nav-fixes`

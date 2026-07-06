# 需求确认

## 讨论ID: 2026-07-07-longpress-no-led-toggle

## GPIO仲裁层修复

- **控制目标**: 修复长按GP30进入菜单时RGB灯错误切换
- **修改范围**: 仅 `src/addons/fightpad_ambient_leds.cpp` 的 `readControls()` 函数
- **修改内容**: 删除 `g_scrollWheelButtonBusy` 的 3 行抑制代码
- **安全保证**:
  1. `handleControlEdges()` 已使用 release-edge 处理 ONOFF (避免按下时触发)
  2. `process()` 中 `g_scrollWheelMenuActive` 检查已阻止菜单内的 DIP control edge 处理

## 行为验证

| 场景 | 预期 |
|------|------|
| GP30 短按 <3s | 释放时 LED 切换 |
| GP30 长按 ≥3s | LED 不变，仅进入菜单 |
| 菜单内 GP30 短按 | `g_scrollWheelMenuActive` 已跳过 DIP，LED 不变 |
| 正常 DIP 操作 | PREV/NEXT 正常切换效果 |

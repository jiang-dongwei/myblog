# 子流程拆分

## 讨论ID: 2026-07-07-longpress-no-led-toggle

## 子流程

### S2-A: 删除 readControls() 中 g_scrollWheelButtonBusy 抑制

- **所属子系统**: GPIO仲裁层
- **开发内容**: 删除 `FightpadAmbientLEDAddon::readControls()` 中对 `g_scrollWheelButtonBusy` 的 3 行检查代码
- **前置条件**: 无
- **验证方式**: 长按 GP30 3s 进入菜单，观察 RGB 灯是否不变；短按 GP30 确认 LED 正常切换
- **优先级**: 1

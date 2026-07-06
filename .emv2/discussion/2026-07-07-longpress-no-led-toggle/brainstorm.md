# 头脑风暴

## 讨论ID: 2026-07-07-longpress-no-led-toggle

**跳过**: 此修复为简单 bug 修复（删除 3 行代码），无技术难点。

## 为什么 g_scrollWheelButtonBusy 是多余的

1. `handleControlEdges()` 第352行已使用 **release edge** 处理 ONOFF — 按下时不会触发
2. `process()` 第227行已有 `g_scrollWheelMenuActive` 检查 — 菜单激活后跳过整个 DIP 逻辑
3. 长按 ≥3s 时 `g_scrollWheelMenuActive` 在释放前就已设为 true → 释放时的 edge 不会到达 `handleControlEdges`

`g_scrollWheelButtonBusy` 的原始意图是"在长按判定前就阻止 DIP 响应"，但它引入的假释放边沿正好造成了它想避免的问题。

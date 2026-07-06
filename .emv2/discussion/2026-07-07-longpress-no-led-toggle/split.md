# 需求拆分

## 讨论ID: 2026-07-07-longpress-no-led-toggle

## 子系统列表

### 1. GPIO仲裁层修复
- 类型: 控制
- 简述: 修复 `FightpadAmbientLEDAddon::readControls()` 中 `g_scrollWheelButtonBusy` 抑制导致假释放边沿的问题

## 根因

`readControls()` 使用 `g_scrollWheelButtonBusy` 抑制 GPIO30 读取，导致 `controls`(当前,无ONOFF位) 和 `lastControls`(上次,有ONOFF位) 不一致，`handleControlEdges()` 检测到假的 release 边沿，错误触发 LED 切换。

## 修复

删除 `readControls()` 中 `g_scrollWheelButtonBusy` 的 3 行抑制代码，使 GPIO30 读取始终反映真实物理状态。

# 技术方案

## 跨核状态

直接在 Core0 同时读取 `batteryPercentValid` 和 `batteryPercent` 会遇到 Core1 每轮采样先清除 valid 的短窗口。采用由 BQ27220 成功 SOC 读数更新的单字节锁存状态；读取失败不改变状态。

## 灯效覆盖位置

在 `FightpadAmbientLEDAddon::render()` 先 `clearFrame()`，再检查低电状态并提前返回。此位置早于 Base/Key 效果和 Key Flash 写帧，返回后统一 `show()` 将两条链发送为全黑。

## 恢复方式

低电状态解除后不恢复旧帧缓存，而是按当前菜单状态重新渲染，因此低电期间的菜单修改仍能在恢复后生效，也不会写入额外持久化状态。

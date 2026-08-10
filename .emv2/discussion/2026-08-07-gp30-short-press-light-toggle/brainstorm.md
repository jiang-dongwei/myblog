# 实现方案

## 已确认方案：当前 GP30 状态机发布短按动作

- `ScrollWheelMenuAddon` 继续作为 GP30 短按/长按的唯一判定者。
- `navSelect()` 在菜单关闭时不再直接返回，而是切换独立的“手动灯光允许”状态并保存到 Flash。
- 菜单打开时，`navSelect()` 保持原有菜单确认行为。
- `BTN_LONG` 与 `btnFromLong` 保持现状，长按释放不调用短按动作。

## 最终输出门控

- 在 `FightpadAmbientLEDAddon::show()` 的最终输出判断中加入手动灯光允许状态。
- 手动关闭时先发送 GP22/GP40 最终黑帧，再走现有 GP24 断电逻辑。
- 手动关闭只禁止普通灯效；`bluetoothStatusLightRequired` 保持现有临时 GP40 覆盖和 GP24 供电能力，不改变 OLED 蓝牙状态事件。
- BQ27220 低电保护继续参与最终输出判断，优先级不降低。

## 不采用的方案

### 恢复旧版 `handleControlEdges()`

- 会让灯光插件再次独立读取 GP30，与菜单状态机形成双消费者。
- 存在长按窗口内误触发短按、释放边沿竞争和状态不同步风险。

### 直接复用 `g_menuRgbPowerEnabled`

- 会混淆持久化 `All OFF` 与临时手动关灯。
- 可能破坏恢复前灯效和重启语义。

## 可靠性约束

- 独立状态通过 `FightpadAmbientLEDOptions.manualLightEffectsEnabled` 持久化，旧配置缺少字段时默认允许灯光。
- 每次完整短按只翻转一次；长按不翻转。
- 原有统一 Light Effect、亮度、颜色、Key Flash、蓝牙状态和低电逻辑不被重置；蓝牙提示结束后回到手动关闭状态。

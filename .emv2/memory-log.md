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

## 2026-07-09: Chase 动态变色修复规划 (S6)

### 问题

RGB Customize 中选择 Chase 后，RGB 灯只显示白色，不再动态变色。
用户确认 Chase 应固定为动态变色效果，不要固定白色。

### 决策

1. **菜单选择策略**: 选择 Chase 时清空对应颜色覆盖，避免新配置继续保存 White/static color。
2. **修复范围**: 同时覆盖 GP22 按键灯链与 GP40 环境灯链。
3. **旧配置兼容**: 渲染层按 effect 类型决定颜色来源；Chase 忽略 static color override，Static Color 保持使用用户选色。
4. **硬件范围**: 纯软件修复，不新增引脚，不改变 GP22/GP40/GP30-GP32/GP19。

### 讨论ID
`2026-07-09-chase-color-cycle-fix`

### 实现记录

- `ScrollWheelMenuAddon::navSelect()` 中移除非静态效果自动改 White 的逻辑。
- 选择 Button/Ambient 的 Chase 时，对应清空 `g_menuRgbTop` / `g_menuRgbBottom` 为 `0xFF`。
- `FightpadAmbientLEDAddon::renderAmbient()` 和 `renderButtons()` 的 Chase 分支改为 `RGB::wheel()` 动态颜色。
- 旧配置中保存的 White/static color 不再影响 Chase 渲染；Static Color 分支仍继续使用用户选色。

## 2026-07-09: Base Chase 平滑亮度梯度规划 (S7)

### 需求

菜单 `RGB Customize -> Base Effect -> Chase` 中，GP40 环境灯追逐效果需要首尾灯暗一些、中间灯亮一些，让视觉上的追逐更流畅。

### 决策

1. **修改范围**: 先改 GP40 Base Chase；随后按用户追加要求同步修改 GP22 Button Chase。
2. **光带长度**: Chase 光带从 4 颗改为 5 颗。
3. **亮度梯度**: 使用 `0.05, 0.25, 0.80, 0.25, 0.05`。
4. **颜色来源**: 继续使用 S6 的 `chaseColorFor()` 动态颜色，每颗灯按自身位置取色后乘亮度。
5. **硬件范围**: 纯软件视觉效果调整，不新增引脚、不改 PIO/LED 数量。

### 讨论ID
`2026-07-09-base-chase-smooth-gradient`

### 实现记录

- `FightpadAmbientLEDAddon::renderAmbient()` 中 `AL_CUSTOM_EFFECT_CHASE` 从 4 灯光带改为 5 灯光带。
- GP40 Base Chase 梯度改为 `0.05, 0.25, 0.80, 0.25, 0.05`。
- 继续使用 `chaseColorFor()` 动态颜色，每颗灯按位置取色后应用亮度。
- `renderButtons()` 的 GP22 Button Chase 使用 3 灯亮度梯度。
- 修复 GP22 Button Chase 偶发未完整循环就重头开始的问题：GP40 Base Chase 与 GP22 Button Chase 拆分为独立的 chase 位置与计时状态。
- 进一步修正 GP22 Button Chase 的边界视觉问题：`buttonChasePixel` 独立按物理顺序完整经过 `R2 -> B -> A -> UP -> LEFT`。
- GP22 Button Chase 视觉方向改回先亮后暗：新进入的灯为当前最高亮度，随后变暗。

## 2026-07-09: Static Color 与 Breathing 拆分 (S8)

### 需求

Base Effect 中 Static Color 选择颜色后保持固定颜色和固定亮度。
Key Effect 中新增 Breathing 选项，把原 Static Color 的呼吸灯行为移动到 Breathing；
Key Static Color 选择颜色后固定颜色和亮度。

### 决策

1. **兼容旧索引**: Key Effect 保持 `0=Static Color, 1=Rainbow, 2=Chase, 3=Static Theme` 不变，新加 `4=Breathing`，避免旧配置错位。
2. **固定亮度**: Static Color 使用固定 `0.5f` 亮度，接近原呼吸效果平均亮度。
3. **Key Breathing 颜色**: 复用当前 GP22 选色 `g_menuRgbTop`，未选择时默认 White。
4. **Base Breathing**: 保持原有 Base Effect 的 Breathing 行为不变，仅修改 Base Static Color。

### 讨论ID
`2026-07-09-static-color-breathing-split`

### 实现记录

- `kMenuButtonEffects` 新增 `Breathing`，effect index 为 `4`。
- `renderAmbient()` 的 Static Color 改为固定选中颜色、固定亮度。
- `renderButtons()` 的 Static Color 改为固定选中颜色、固定亮度。
- `renderButtons()` 新增 Breathing 分支，承接原 Static Color 的呼吸亮度逻辑并保留按键闪灯覆盖。

## 2026-07-09: Breathing Rainbow 与 Breathing Color 菜单拆分 (S9)

### 需求

Base Effect 和 Key Effect 都需要同时提供两个呼吸灯入口：

- `Breathing Rainbow`: 直接选中后自动改变颜色并呼吸。
- `Breathing Color`: 进入下一级颜色菜单，选择单色后以该颜色呼吸。

### 决策

1. **命名**: 使用 `Breathing Rainbow` / `Breathing Color`，明确区分自动变色和单色可选。
2. **索引兼容**: 保持已有 Key/Base effect index `0..4` 尽量不变；新增自动/单色呼吸使用 index `5` 补齐。
3. **颜色菜单隔离**: 新增 `COLOR_BTN_BREATH` 与 `COLOR_AMB_BREATH` 层级，避免 Breathing Color 选色时误切回 Static Color。

### 讨论ID
`2026-07-09-breathing-rainbow-color-menu`

### 实现记录

- Key Effect 菜单新增 `Breathing Rainbow`，`Breathing Color` 改为进入颜色选择。
- Base Effect 菜单将原 `Breathing` 改名为 `Breathing Rainbow`，并新增 `Breathing Color` 颜色选择入口。
- `renderAmbient()` 新增 Base 单色呼吸 effect index `5`。
- `renderButtons()` 新增 Key 自动变色呼吸 effect index `5`，原 Key 单色呼吸保留在 index `4`。

## 2026-07-09: RGB Effect 菜单精简 (S10)

### 需求

- Key Effect 删除 `Static Theme`。
- Key Effect 中 `Breathing` 进入颜色菜单，颜色列表包含 `OFF` 与各种颜色，用于单色呼吸。
- Base Effect 删除自动变色 `Breathing` 菜单项。
- Base Effect 中 `Breathing` 进入颜色菜单，颜色列表包含 `OFF` 与各种颜色，用于单色呼吸。
- Base Effect 中 `Static Theme` 改名为 `Rainbow`。

### 决策

1. **只删除菜单入口**: 底层旧 effect case 保留，避免 flash 中旧 index 造成异常。
2. **Key Breathing**: 菜单名使用 `Breathing`，进入 `COLOR_BTN_BREATH`，选色后写入 effect index `4`。
3. **Base Breathing**: 进入 `COLOR_AMB_BREATH`，选色后写入 effect index `5`。
4. **Base Rainbow**: 复用原 Static Theme 渲染 index `4`，仅菜单名称改为 `Rainbow`。

### 讨论ID
`2026-07-09-rgb-effect-menu-trim`

### 实现记录

- `kMenuButtonEffects` 删除 `Static Theme` 和 `Breathing Rainbow` 菜单入口。
- `kMenuButtonEffects` 中 `Breathing` 进入颜色菜单，可选 `OFF` 与各颜色。
- `kMenuAmbientEffects` 删除自动变色 `Breathing Rainbow` 菜单入口。
- `kMenuAmbientEffects` 中 `Breathing` 进入颜色菜单，可选 `OFF` 与各颜色。
- `kMenuAmbientEffects` 中原 `Static Theme` 改名为 `Rainbow`。
- 修复 OLED 菜单渲染遗漏：`DisplayAddon::drawScrollWheelMenu()` 现在识别 `COLOR_BTN_BREATH` / `COLOR_AMB_BREATH`，Breathing 下一级颜色菜单不再显示异常，并能正确标记当前颜色。
- 更新 `FightpadAmbientLEDOptions` 注释，effect index 范围从 `0-4` 修正为 `0-5`。
- Base Effect 的 `Rainbow` 不再复用旧 Static Theme 色表，改为与 Key Effect `Rainbow` 同类算法：每颗灯按 LED 序号分配不同色轮相位，并随 `wheelFrame` 滚动。

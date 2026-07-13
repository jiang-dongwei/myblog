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

## 2026-07-10: BQ27220 电量 SOC/FCC 稳定性规划 (S11)

### 现象

- OLED 显示 75% 时电压约 3781mV。
- 电量到 52% 后直接跳变到 7%。
- FCC 从 650 跳变成 522。
- 5% 电量时电压约 3556mV，0% 时约 3512mV。
- 电量耗尽后灯乱闪，RP2350/ESP32 反复重启，重启后 FCC 又恢复到 650。

### 约束

- BQ27220 直接电池供电，不会跟随 RP2350/ESP32 系统重启。
- BQ27220 最低工作电压约 2.4V，低于产品电池截止电压 2.75V。
- GP24 会影响 RP2350 和 ESP32，但不影响 BQ27220。
- 不实现串口日志、Flash 日志或 ESP32 侧日志。
- 不改 ESP32 相关代码。
- 不改 OLED 正常显示。
- 低电时仍保持正常 BQ 配置流程。

### 决策

1. 先审计当前 BQ27220 开机配置写入行为，重点确认 FCC 是否被每次写回 650。
2. 使用现有 OLED 诊断页进行人工记录，不新增日志。
3. S11-C 是实机复测流程，不是代码功能；用 SOC/V/I/FCC/状态码记录关键跳变点。
4. 低电保护只评估 RP2350 灯效侧降耗，不改 OLED、不改 ESP32、不直接关 GP24。

### S11-A 审计记录

- `FIGHTPAD12SLIM_BQ27220_CONFIGURE_RAM = 1` 时，RP2350 每次重启后都会重新尝试 BQ27220 RAM 配置。
- 当前配置写入列表包含 `BQ27220_DATA_FULL_CHARGE_CAPACITY`。
- `BQ27220_DATA_FULL_CHARGE_CAPACITY` 的目标值来自 `FIGHTPAD12SLIM_BQ27220_DESIGN_CAPACITY_MAH = 650`。
- 如果 BQ27220 学习/计算出的 FCC 已经变为 522，RP2350 重启后配置流程会把 FCC 写回 650。
- 这与低电 brownout 重启后 FCC 恢复 650 的实测现象吻合。

### S11-B 审计记录

- 当前 BQ27220 addon 已读取 SOC、Voltage、Current、FCC、ReadStatus、Security Status 和 Data Memory Debug 信息。
- 当前 OLED 诊断页已显示 SOC、V、I、FCC、错误码和 Security 状态简码。
- 在“不实现日志、不改 OLED 正常显示、不改 ESP32”的约束下，S11-C 人工复测所需字段已经具备。
- S11-B 无需新增固件代码。

### 讨论ID
`2026-07-09-bq27220-battery-soc-fcc-stability`

## 2026-07-10: RP2350B 固件信息菜单规划 (S12)

### 需求

- 用户通过原理图确认主控为 RP2350B。
- 将菜单现有 `RP2350B FW Version` 信息页从占位内容改为真实固件信息。
- 显示 Pico SDK 版本、平台、板级配置和 Cortex-M33。

### 决策

1. 直接在 Core1 的 `DisplayAddon::drawScrollWheelMenu()` 中读取编译期版本宏。
2. 不扩展 Core0/Core1 共享菜单状态，不写 Flash。
3. 使用 22 字节缓冲区限制每行最多 21 个可见字符。
4. 保持 INFO 页拨轮禁用、短按返回的现有导航行为。
5. 不修改 ESP32 INFO 页和其他菜单。

### 讨论ID

`2026-07-10-rp2350-firmware-info-menu`

### 实现记录

- `DisplayAddon::drawScrollWheelMenu()` 的 RP2350B INFO 分支使用 `PICO_SDK_VERSION_STRING`、`GP2040PLATFORM` 和 `GP2040_BOARDCONFIG`。
- 页面显示 RP2350B、Pico SDK、Cortex-M33 和当前目标信息；不显示 Git FW、Build ID 和 Debug/Release 类型。
- 动态字段使用 22 字节缓冲区和格式精度限制，最多显示 21 个字符。
- ESP32 和 RGB INFO 页继续保留原有占位显示。
- `git diff --check` 通过；按用户约定未编译，S12-C 等待用户实机验证。

## 2026-07-13: ESP32-C6 固件信息串口接收与菜单显示规划 (S13)

### 需求

- 复用 RP2350B UART0 GP44/GP45 接收 ESP32-C6 UART0 GPIO16 发出的固件信息帧。
- 按 `docs/fw_info_protocol_rp2350.md` 重组多帧 Payload，解析 SDK、Plat、Board、CPU。
- 在现有 ESP32 INFO 页显示真实信息；未接收到完整有效数据时显示用户指定文本 `Coming to soon`。

### 决策

1. UART RX 在关闭 CDC 时也必须始终运行，统一入口同时承担固件解析和可选 CDC 转发。
2. 使用 `0x46 0x49` 滑动同步、XOR、flag、连续 seq、200ms 超时和 256 字节边界检查。
3. 四个必需字段齐全后才发布；无效新序列不清除上一份有效信息。
4. Core0 完整解析后通过版本化跨核快照供 Core1 OLED 使用，不写 Flash。
5. Board 在 OLED 上占两行，每行最多 21 字符；不改变 INFO 页导航。
6. 不主动复位 ESP32、不增加请求重发协议；漏接启动帧时保持 `Coming to soon`。
7. 按仓库约定不运行编译，由用户完成构建和实机验证。

### 讨论ID

`2026-07-13-esp32-firmware-info-uart`

### 实现记录

- `FightpadESP32ProxyAddon::process()` 现在无论 CDC 是否启用都会读取 UART RX；启用 CDC 时仍保留收到字节的转发副本。
- 新增 `0x46 0x49` 滑动同步器、8 字节 XOR 校验、FIRST/MIDDLE/LAST/SINGLE、连续 seq、200ms 超时和 256 字节 Payload 边界处理。
- Payload 只在 SDK、Plat、Board、CPU 四个字段全部有效时发布；坏帧或无效新序列不会清除上一份有效数据。
- Core0/Core1 共享信息使用 Pico critical section 和版本号保护完整结构快照。
- ESP32 INFO 页显示标题、SDK、Plat、两行 Board、CPU 和 Back；无有效数据时显示 `Coming to soon`。
- 协议示例静态生成结果为 66 字节、17 帧，FIRST=`0xC0`、LAST=`0x50`，全部 XOR 通过，Board 可在两行内显示。
- `git diff --check` 通过；按仓库约定未编译，S13-D 等待用户构建和实机验证。

## 2026-07-13: Key Effect Gradient 规划与实现 (S14)

### 需求

- 在 `RGB Custom -> Key Effect` 中增加 Base Effect 已有的 `Gradient`。
- GP22 的 12 颗按键灯显示相同的动态色轮颜色。
- 保留按键 Key Flash 覆盖和重启后的效果持久化。

### 决策

1. Key Gradient 使用新效果编号 `6`，不改变已有 Key 效果编号。
2. Key Gradient 使用独立的色轮位置和反向状态，避免与 Base 动态效果相互改变速度。
3. 算法与 Base Gradient 一致：`RGB::wheel()`、固定亮度 `0.5f`、每帧步进 `2`、边界往返。
4. Gradient 忽略但不清空已保存的 Static Color，切回 Static Color 后恢复原颜色。
5. 每颗灯继续检查 `gp22FlashUntil`，Key Flash 有效时优先显示 Flash 颜色。
6. 按仓库约定不运行编译，由用户完成构建和实机验证。

### 讨论ID

`2026-07-13-key-effect-gradient`

### 实现记录

- `kMenuButtonEffects` 在 Static Color 后增加 `Gradient`，目标效果编号为 `6`。
- `FightpadAmbientLEDAddon::renderButtons()` 新增 Key Gradient 分支。
- 新增 `buttonGradientFrame` / `buttonGradientReverse`，与 Base 的共享色轮状态隔离。
- `FightpadAmbientLEDOptions.buttonEffectIndex` 注释范围更新为 `0-6`；字段和持久化路径不变。
- `git diff --check` 和定向源代码检查通过；按仓库约定未编译，S14-D 等待用户构建和实机验证。

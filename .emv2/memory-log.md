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

## 2026-07-13: BQ27220 启动配置回读与电流校准规划 (S15)

### 需求

- 启动实际回读 ITPOR、Battery ID、Battery Low、EDV0/1/2、CC Offset、Board Offset、CC Gain 和 CC Delta。
- Battery Low 与 EDV 不一致时选择性修复，保留修复前值并二次回读。
- 层级 0 新增 `Battery Info`，使用四页 OLED 显示运行数据、EDV配置、校准参数和充电终止状态。
- 采样电阻为 10 mOhm，标称校准电流最终确定为 318 mA。

### 决策

1. Battery ID 第一轮只回读，不自动修改。
2. 普通 RP2350 重启不得重写 Learned FCC；只有确认 BQ27220 RAM 恢复默认时才恢复 FCC 初始基线。
3. CC Gain/CC Delta 第一轮只回读并同时显示解码值与原始 F4 字节，实测后再固化板级校准值。
4. 318 mA 只是负载目标，校准计算必须使用外部电流表实际测量值。
5. Battery Info 使用独立菜单层级，GP31/GP32 和 GP30 短按翻页，GP19 返回。
6. 保持用户原有 `BUTTONS` 初始页及电池诊断显示不变；新增完整回读信息从菜单进入。
7. 按仓库约定不运行编译，由用户完成构建和实机验证。

### 讨论ID

`2026-07-13-bq27220-config-readback-calibration`

### 实现记录

- 启动时实际回读 Battery ID、Battery Low、EDV0/1/2、CC Offset、Board Offset、CC Gain 和 CC Delta；F4 同时保存原始字节和解码值。
- TI 公开的 BQ27220 `BatteryStatus()` 位表没有独立 ITPOR 位，固件不猜测位掩码，改用 Design Capacity/默认配置证据显示 `RAM:INIT` 或 `RAM:KEEP`。
- Battery Low、EDV0/1/2 和受控 CEDV 位按差异选择性修复，并保存修复前、目标和修复后二次回读结果。
- 完整 RAM 初始化仍只在默认恢复证据成立时执行；普通 RP2350 重启不写 learned FCC。
- 层级 0 新增 `Battery Info`，四页显示运行数据、配置检查、CC 校准数据和充电终止状态；用户原有 `BUTTONS` 初始页保持不变。
- 充电终止目标改为 `Taper Current=200mA`、`Taper Voltage=50mV`；启动实际回读 Charging Voltage、Taper Current、Taper Voltage 和 SOC Flag Config A，仅在不一致时使用 `EXIT_CFG_UPDATE(0x0092)` 局部修复，不重置 learned FCC 或 CC 校准值。
- 第四页显示充电参数实际回读、瞬时 Current、`AverageCurrent(0x14)` 和 BatteryStatus 的 FC/TCA，用真实平均电流验证Taper窗口。
- Fightpad12Slim 板级采样电阻记录为 10 mOhm，校准负载记录为 318 mA，当前保持 `CAL:UNCAL`，等待至少 5 组外部电流表/BQ 实测数据后再计算并写入 CC Gain/CC Delta。
- `git diff --check` 和定向源码审计通过；按仓库约定未运行编译，S15-D 等待用户构建和实机采样。

## 2026-07-14: BQ27220 电量档位串口快照 (S16)

### 需求

- 使用 RP2350B 的 GPIO42/GPIO43 增加电量信息串口输出。
- SOC 到达 100%、75%、50%、25%、15%、10%、7%、3%、0% 时各触发一次 Battery Info 快照。

### 决策

1. 使用 UART1、115200 8N1，GP42=TX、GP43=RX；RP2350 高 GPIO 必须通过 `UART_FUNCSEL_NUM()` 选择 AUX UART 复用，不能直接使用普通 `GPIO_FUNC_UART`。
2. 串口输出按 P1-P4 组织并覆盖 Battery Info 四页字段，包括运行值、EDV/Battery 配置、CC 校准和充电终止状态。
3. BQ27220 每 2 秒轮询一次，仅在本轮 SOC 精确等于目标档位时触发，不用相邻档位的数据补记两次轮询间跳过的目标。
4. 本次启动期间，连续停留或短暂离开后回到同一目标不重复；命中另一个目标档位后才允许未来再次记录此前档位。RP2350 重启后去重状态清零。
5. UART0 GP44/GP45 继续供 ESP32 proxy 使用；当前 UART1 无运行时冲突，但未来 GP36-GP39 BT HCI 不可与本日志同时占用 UART1。
6. 按仓库约定不运行编译，由用户完成构建和串口实机验证。

### 讨论ID

`2026-07-14-bq27220-battery-uart-logging`

### 实现记录

- BoardConfig 启用 BQ27220 日志 UART1，固定 GP42 TX、GP43 RX、115200 8N1。
- BQ27220 每轮采样完成后检查目标 SOC，使用固定栈缓冲输出，未使用动态内存。
- 日志使用 `[BATTERY]`/`[/BATTERY]` 包围，P1-P4 覆盖 OLED Battery Info 页面字段并增加 TRIGGER/UPTIME/有效性标签；无效字段显示 `NA`、`?` 或 `WAIT`。
- 目标档位为 100/75/50/25/15/10/7/3/0%，同一目标连续采样只打印一次。
- 已完成定向静态检查；按仓库约定未编译，S16-C 等待用户构建和实机验证。

## 2026-07-14: 7% 低电灯光关闭 (S17)

### 需求

- BQ27220 电量降低到 7% 时关闭 GP22 的 12 颗按键灯和 GP40 的 19 颗环境灯。
- 低电时所有 Base/Key 效果和 Key Flash 都不能重新点亮 LED。

### 决策

1. 使用 `SOC <= 7%`，覆盖直接从 8% 跳到 6%、3% 或 0% 的情况。
2. BQ27220 Core1 只在 SOC 成功读回时更新原子保护状态；读取失败保持上一次状态。
3. 启动尚未得到有效 SOC 时不猜测低电，首次有效样本小于等于 7% 后再关闭。
4. 在 `render()` 提前返回减少无用效果计算，并在两条灯链共同的最终 `show()` 写入点再次清零，防止启动、诊断或 Key Flash 路径越过保护。
5. 保护只覆盖输出帧，不修改 `enabled`、菜单颜色、效果编号或 Flash 配置；有效 SOC 回升到 8% 后自动恢复当前灯效。
6. 只关闭 31 颗 WS2812 的发光，不切断 GP24 升压供电，也不关闭独立的 GP23 状态灯。
7. 按仓库约定不运行编译，由用户完成构建和实机验证。

### 讨论ID

`2026-07-14-low-battery-led-cutoff`

### 实现记录

- BoardConfig 新增 `FIGHTPAD12SLIM_BQ27220_LIGHTS_OFF_PERCENT=7`。
- BQ27220 采样端新增 `std::atomic_bool` 低电灯光保护状态和只读接口，使用 release/acquire 跨核发布。
- `FightpadAmbientLEDAddon::render()` 在全黑帧初始化后检查保护状态；`show()` 在 GP40/GP22 `SetFrame()` 前执行最终清零。
- 通用 `NeoPicoLEDAddon` 继续由 `FIGHTPAD12SLIM_AMBIENT_OWNS_GP22=1` 禁用，不存在 GP22 后写覆盖。
- 静态源代码检查和 `git diff --check` 通过；按仓库约定未编译，S17-C 等待 8%/7%/3%/0% 与充电恢复实机验证。

## 2026-07-15: 电量周期串口、OLED休眠与主页面电量格 (S18)

### 需求

- GP42/GP43 电量快照从 SOC 档位触发改成每 2 秒发送。
- GP30/31/32 连续 1 分钟无操作后 OLED 休眠，任一输入再次变化时唤醒。
- BUTTONS 主页面移除 SOC/V/I/FCC 诊断覆盖，保留右上角四格电池图标和右下角 SOC 百分比。

### 决策

1. 串口复用 BQ27220 的 2000 ms 轮询节拍，每轮均输出 P1-P4；单字段读取失败仍输出本轮并以 `NA/?/WAIT` 标记。
2. Core0 在 GP30/31/32 原始边沿更新原子活动时间戳，Core1 DisplayAddon 读取时间戳控制 `setPower()`；不让显示侧重复读取或去抖 GPIO。
3. 休眠期间停止 OLED 绘制和 I2C 刷帧；首次唤醒边沿不消费，原按键/拨轮处理继续执行。
4. 复用已有四格电池图标、SOC 到 0..4 格映射和百分比绘制函数；图标保持右上角，百分比保持右下角，完整诊断数值从 Battery Info 菜单查看。
5. GP19 不计入本次专用空闲计时；Battery Info 四页、7% 关灯和 RGB 行为不变。
6. 按仓库约定不运行编译，由用户完成构建和实机验证。

### 讨论ID

`2026-07-15-battery-uart-oled-sleep-icon`

### 实现记录

- 删除 UART 的 SOC 档位表、档位判断和 `lastLoggedBatteryLevel` 去重状态；每次 BQ27220 轮询完成后输出一组 `[BATTERY]` P1-P4。
- 周期日志头改为 `PERIOD:2000ms`；SOC 读取失败时与其他字段一样输出 `NA`，不跳过整组日志。
- GP30/31/32 原始边沿通过 `std::atomic<uint32_t>` 发布最后活动时间；DisplayAddon 在 60000 ms 空闲后调用驱动 `setPower(false)` 并跳过渲染，首次新边沿自动上电。
- BUTTONS 关闭全屏 BQ 数值诊断，四格电池图标保持像素 `(99,0)`，状态栏保留前 14 个字符；右下角继续显示 SOC 百分比，原按键布局和 Battery Info 四页保留。
- `git diff --check`、旧档位状态残留搜索和定向源码审计通过；按仓库约定未运行编译，S18-D 等待用户构建和实机验证。

## 2026-07-15: 隐藏 Battery Info 层级 0 入口 (S19)

### 需求与决策

- 正常菜单的层级 0 不再显示 `Battery Info`，但四页诊断代码需要保留供以后调试。
- 新增 `SCROLLWHEEL_BATTERY_INFO_MENU_ENABLED`；默认值为 1，Fightpad12Slim 板级配置设为 0。
- 该宏只条件编译 `kMenuMain` 中的入口，不删除 `SWMenuLevel::BATTERY_INFO`、四页绘制、翻页或返回逻辑。
- `kMenuMainCount` 继续由数组大小自动计算；隐藏入口后 RP2350B、ESP32C6、RGB Customize 的导航不依赖旧索引。
- 按仓库约定不运行编译，由用户完成构建和实机验证。

### 讨论ID

`2026-07-15-hide-battery-info-menu`

## 2026-07-15: RGB Customize 三档亮度控制规划 (S20)

### 需求

- 在 `RGB Customize` 子菜单增加 `Brightness`。
- 提供 `Bright=0.5f`、`Normal=0.3f`、`Dim=0.1f` 三档，不提供 OFF。
- 一个档位同时控制 Key Effect 与 Base Effect 的 Static Color、Gradient、Rainbow。

### 决策

1. Flash 只保存 `0/1/2` 档位编号，在渲染时转换为浮点亮度；旧配置默认 Bright。
2. 短按 GP30 立即应用并保存，停留在亮度列表，GP19 返回；当前档位显示 `*`。
3. Key Flash 保持 `0.8f`，Chase 与 Breathing 保持各自算法，7% 低电关灯保持最高优先级。
4. Brightness 插入 Base Effect 与 All OFF 之间，并使用命名索引避免原 All OFF 裸索引失效。
5. 按仓库约定不运行编译，由用户完成构建和实机验证。

### 讨论ID

`2026-07-15-rgb-brightness-levels`

### 实现记录

- `FightpadAmbientLEDOptions` 新增 `brightnessLevel=6`，默认 `0=Bright`，启动时对越界值回退为 Bright。
- `RGB Customize` 顺序变为 Key Flash、Key Effect、Base Effect、Brightness、All OFF；Brightness 提供 Bright/Normal/Dim，并显示当前档位 `*`。
- 短按 GP30 立即保存并停留在亮度列表，GP19 返回 RGB Customize；All OFF 使用命名索引 4，避免与新增项冲突。
- GP22/GP40 的 Static Color、Gradient、Rainbow 使用共享 `0.5f/0.3f/0.1f`；Key Flash 保持 `0.8f`，Chase/Breathing 和 7% 低电保护不变。
- 定向源码检查和 `git diff --check` 通过；按仓库约定未运行编译，S20-E 等待用户构建和实机验证。

## 2026-07-15: 呼吸灯峰值亮度调整

- Key/Base 当前菜单 Breathing 的范围从 `0.02f~1.0f` 改为 `0.02f~0.5f`，完整周期保持 `2400ms`。
- 旧 Base Breathing Rainbow 隐藏分支峰值同步改为 `0.5f`，步进从 `0.008f` 降为 `0.004f`，保持原有往返节奏。
- Key Flash 继续使用 `0.8f`，因此按键闪灯始终高于呼吸灯峰值。
- `git diff --check` 通过；按仓库约定未运行编译。

## 2026-07-15: RGB关闭时GP24电源门控 (S22)

### 原理图纠正

- 重新检查 `22-FIGHTPAD_20260625-schematic_new.pdf`：GPIO24/`5V_EN` 只连接 FP6276 的 EN，输出为 RGB 使用的 `VCC_5V`。
- RP2350 与 ESP32 使用独立的 3.3V 电源路径；旧 S11 讨论中“GP24 会同时影响 RP2350/ESP32”的记录不适用于当前 20260625 原理图版本。

### 实现决策

- `FightpadAmbientLEDAddon` 保持为 GP24 唯一写入者；菜单只发布运行时 RGB 电源请求。
- `All OFF` 先向 GP22/GP40 两条灯链发送最终全黑帧，等待 1ms 后拉低 GP24。
- 选择非黑颜色或动态灯效时拉高 GP24，等待 5ms 后发送恢复帧；选择 Brightness 不会单独唤醒灯光。
- Flash 继续用既有“三个黑色 + 两个默认效果”表示 `All OFF`，启动时据此恢复门控状态，不增加配置字段。
- 7% 低电强制关灯复用同一门控路径，SOC 恢复后自动重新上电并恢复当前效果。
- 板级宏 `FIGHTPAD12SLIM_AMBIENT_POWER_GATE_WHEN_OFF` 可快速停用硬门控并回退为只发送黑帧。
- GP24宏引用搜索确认只有 `FightpadAmbientLEDAddon` 初始化和门控函数写入；菜单与低电状态路径检查、`git diff --check` 通过。
- 按仓库约定不运行编译，由用户完成构建和实机验证。

### 讨论ID

`2026-07-15-rgb-gp24-power-gating`

### S22-E首次实测失败

- 用户关闭RGB后测量GP24仍为3.3V，没有拉低。
- 已记录至 `.emv2/checkpoints/HVR-S22-001.md`，S22-E进入返工。
- 当前无新增串口日志，后续检查All OFF请求传播、实际板级宏和GP24最后写入者。
- 根因核对：build目录的ELF/UF2为16:22旧产物，相关OBJ为16:21；GP24门控源码在17:57至18:04才修改，旧ELF也没有新门控符号。
- 当前不再改动门控状态机；等待用户重新构建、确认UF2时间戳更新并烧录后复测。

## 2026-07-20: 蓝牙连接状态弹窗与 Base 灯效 (S23)

### 需求与决策

- ESP32-C6 通过 UART0 发送固定 8 字节 `0x46 0x53` 蓝牙状态帧，Byte0~6 XOR，状态为 Disconnected/Connecting/Connected/Pairing。
- Connecting/Pairing 立即唤醒 OLED 并持续覆盖当前页面；Connected/Disconnected 显示 1000ms，随后恢复弹窗前页面而不改变显示模式或拨轮菜单状态。
- GP40 Base 在 Connecting/Pairing 时显示纯蓝 5 灯 Chase，Connected 时全蓝静态 1000ms，Disconnected 时全黑 1000ms；GP22 Key 灯保持原行为。
- 临时灯效不修改菜单或 Flash 配置，可在 All OFF 时临时唤醒 GP24；BQ27220 `SOC <= 7%` 低电保护仍保持最高优先级。
- `0x00` 无法区分连接失败和普通断连，统一显示 `Disconnected`；Connecting/Pairing 不增加 RP2350 侧超时。

### 实现记录

- 将固件信息专用 UART 同步器扩展为统一 8 字节接收器，识别 `0x49` 和 `0x53`，统一完成滑动同步和 XOR 后按类型分发。
- 合法状态事件通过独立 critical section 发布状态、接收时间和序号，供 Core0 灯效与 Core1 OLED 使用；非法状态或坏校验不更新快照。
- `DisplayAddon::process()` 增加高优先级 `Bluetooth Status` 覆盖页，新事件会唤醒 OLED 并刷新休眠计时，结果到期后自然恢复原页面。
- `FightpadAmbientLEDAddon` 在低电检查之后、普通渲染之前生成 GP40 临时帧；蓝色 Chase 使用事件时间计算位置，不写原 Base 动画变量。
- 最终 `show()` 将 Connecting/Pairing/Connected 作为临时供电请求，Disconnected 不请求点亮；低电最终写入拦截保持不变。
- 新增 `docs/bluetooth_status_protocol_rp2350.md`，记录帧、状态、时序、OLED 和 Base 灯效行为。
- 四种帧 XOR 静态结果为 `0x15/0x14/0x17/0x16`；非法状态和坏校验用例通过，`0x49`/`0x53` 分发、21列 OLED、GP22 隔离、无持久化写入、All OFF 和低电优先级检查通过。
- 协议增补 `0x03 Pairing` 后，OLED 显示 `Pairing...`，GP40 沿用纯蓝 Chase，直到下一状态帧；启动期 ASCII `C6_DONE\n` 由二进制同步器安全忽略，不增加运行时动作。
- `git diff --check` 通过；按仓库约定未运行编译，S23-E 等待用户构建烧录和实机验证。

### 讨论ID

`2026-07-20-bluetooth-status-popup-led`

## 2026-07-21: 菜单期间游戏输入锁定与 ESP32-C6 协同 (S24)

### 需求与决策

- GP30 长按真正打开菜单的同一轮开始锁定 GP2～GP20 的游戏交互；长按判定之前保持原行为。
- GP19 继续通过直接 GPIO 读取执行菜单 `BACK/退出`，但对应的游戏 A2 输出必须被中和；GP20 Turbo 同样不得泄漏或修改配置。
- RP2350 负责菜单锁存、USB 最终 neutral、本地 Turbo/宏/普通及重启热键保护；ESP32-C6 负责 BLE HID 最终 neutral 和独立的 release-to-rearm。
- 退出菜单后不立即恢复：RP2350 要求 GP2～GP20 原始 GPIO 与消抖 GPIO 同时释放并连续稳定 30ms。
- `FP` 继续传输真实 buttons、D-pad、LX、LY，仅在 Byte4 bit7 增加游戏锁标志；按钮 bit15 保持保留，避免旧 ESP32 将其解释成 Button16。
- 本轮只修改 RP2350 工程并编写共享协议文档，不修改 ESP32-C6 源码；按仓库约定不运行编译。

### 实现记录

- `ScrollWheelMenuAddon` 增加 `UNLOCKED/CAPTURED/DRAIN_UNTIL_RELEASE` 三态；菜单打开同轮锁定，菜单关闭同轮进入释放排空。
- USB 驱动处理期间临时替换为 neutral `GamepadState` 并屏蔽 GP2～GP20 的 `debouncedGpio`，处理后恢复真实状态，保证 OLED、灯效与 ESP32 串口快照不被破坏。
- 锁定期间暂停 `gamepad->hotkey()`、重启热键和 Turbo 处理；输入宏执行 `reset()`，防止 toggle/hold 宏在退出后继续。
- Bluetooth `FP` Byte4 使用 `bit7 lock | bits3..0 dpad`；帧长、类型、按钮、轴、发送节拍与 XOR 校验规则保持不变，锁状态变化参与整帧比较并立即发送。
- 新增 `docs/menu_gameplay_lock_protocol_rp2350_esp32.md`，规定 ESP32-C6 的 `UNLOCKED/LOCKED/DRAIN` 状态机、活动检测、`FT`/超时 fail-neutral、GP19/GP20 边界和测试序列。
- 定向调用链审计、已知帧 XOR 校验和 `git diff --check` 通过；未运行编译。S24-E 等待 ESP32 AI 实现后由用户完成双端构建、烧录和实机验证。

### 讨论ID

`2026-07-21-menu-game-input-lockout`

## 2026-07-21: GP30 长按阈值缩短到 2 秒

- `SCROLLWHEEL_LONG_PRESS_MS` 从 `3000` 调整为 `2000`，菜单进入和长按退出共用该阈值。
- 保留原始输入滤波 30ms 和按下状态消抖 30ms，因此从物理按下到触发约为 2060ms，加少量主循环调度误差。
- 同步更新菜单/RGB 子系统说明文档并清理头文件中的重复宏定义；状态机和短按互斥逻辑不变。
- `git diff --check` 通过；按仓库约定未运行编译。

## 2026-07-22: 蓝牙状态双段 Chase 规划 (S25)

### 需求与决策

- 保留蓝牙 Pairing/Connecting 现有单段 5 灯 Chase，并增加板级条件编译开关。
- 新版使用两段纯蓝 Chase，每段 3 颗，头灯到尾灯亮度为 `80%/25%/5%`。
- GP40 共 19 颗灯，第二段头灯固定相对第一段偏移 9 格，形成最接近的对角位置。
- 两段同方向移动，尾灯位于运动方向后方，速度保持 `50ms/格`。
- Fightpad12Slim 默认启用新版；公共默认关闭新版以保留其他板级兼容性。
- Connected/Disconnected、GP22、普通 Base/Key Chase、All OFF 临时唤醒和 `SOC <= 7%` 低电保护不变。

### 讨论ID

`2026-07-22-bluetooth-dual-chase`

### 实现记录

- `BoardConfig.h` 当前将 `FIGHTPAD12SLIM_ESP32_BT_STATUS_DUAL_CHASE` 设为 `1`，公共头文件默认值为 `0`。
- 新分支使用两个相隔 `count / 2`（19 灯时为 9）的 Chase 头，每个头后方两颗形成 `80%/25%/5%` 拖尾。
- 旧分支保留单段 5 灯 `5%/25%/80%/25%/5%` 梯度；新旧分支均为 `50ms/格`。
- 19 个可能头灯位置枚举全部保持 9 格头灯偏移且同时点亮 6 个互不重叠的逻辑灯位。
- 定向状态隔离检查和 `git diff --check` 通过；按仓库约定未运行编译，S25-C 等待用户实机验证。

## 2026-07-31: Fightpad12Slim 量产默认启动 Logo (S26)

### 需求与决策

- 使用用户提供的 `zimo.TXT`，数据为 128×64、1 bpp、逐行、MSB-first、阴码，共 1024 字节。
- 只覆盖 Fightpad12Slim 的 `DEFAULT_SPLASH`，不修改 GP2040-CE 通用默认图和其他板卡。
- 保持静态启动图模式和 3000 ms 显示时间，保持字模原位置和极性。
- 保留 Web Config 的 Logo 覆盖能力；量产新设备直接使用固件默认值，已有持久化配置的设备需恢复出厂或擦除配置后验证。
- 按仓库约定不运行编译，由用户完成构建、烧录和实机验证。

### 实现记录

- 在 `configs/Fightpad12Slim/BoardConfig.h` 内新增板级 `DEFAULT_SPLASH`。
- 将 PCtoLCD2002 的 64 行嵌套输出扁平化为 GP2040-CE 可直接初始化的 1024 字节宏。
- 静态解析确认宏和 `zimo.TXT` 都是 1024 字节且逐字节相同；点亮区域保持 `x=37..81`、`y=14..49`。
- 定向启动图调用链检查和 `git diff --check` 通过；按仓库约定未运行编译。
- S26-C 等待用户构建、烧录和实机验证。

### 讨论ID

`2026-07-31-fightpad-default-splash-logo`

## 2026-07-31: Web Config Fightpad 品牌 (S27)

### 需求与决策

- Web Config 左上角旧资源是包含完整 `GP2040-CE` 字样的 PNG，不是独立图标和文字。
- 使用 S26 启动图中同一个方框 R 轮廓生成透明 SVG，主色沿用 `#ec008c`。
- 导航栏改成独立的 R Logo 和粗斜体 `FIGHTPAD` 文字，方便缩放和后续调整。
- 同步所有语言的品牌名与首页欢迎标题，以及浏览器 title、description、favicon 和 manifest。
- 不全局替换功能说明、协议或兼容性文本中的 `GP2040-CE`。
- 按仓库约定不运行 Web 或固件编译，由用户完成构建烧录和设备页面验证。

### 实现记录

- 新增 `www/public/images/fightpad-logo.svg`，图形轮廓来自 `zimo.TXT` 的 45×36 点亮区域。
- `Navigation.jsx` 使用本地化 `Common:brand-text`，组合 SVG 与 `FIGHTPAD` 文字；导航样式支持亮色和暗色背景。
- 九种语言的 `Common.brand-text` 和 HomePage 欢迎标题改为 `FIGHTPAD`。
- `index.html` 和 `manifest.json` 使用 Fightpad 页面名称和 SVG 图标。
- SVG 解析得到 189 个横向像素段、1064 个点亮像素，与 `zimo.TXT` 裁剪区域逐像素差异为 0。
- 九种语言品牌检查、旧导航 Logo 引用搜索、亮/暗背景临时预览和 `git diff --check` 通过；浏览器连接初始化失败，因此未声明完成实际页面测试。
- S27-D 等待用户构建烧录后检查实际设备 Web Config。

### S27-D首次实测失败

- 用户进入设备 Web Config 后观察到左上角 Logo、右上角模式选项和首页欢迎标题仍显示 GP2040。
- 已记录至 `.emv2/checkpoints/HVR-S27-001.md`，S27-D进入返工。
- 根因是 `Buttons.js` 的模式标签漏改，同时 `www/build` 与 `lib/httpd/fsdata.c` 仍是品牌修改前的旧打包数据；`SKIP_WEBBUILD=TRUE` 不会自动更新它们。
- 返工版本截图显示 SVG Logo 加载失败，但独立的 `FIGHTPAD` 文字正常；按用户决策删除导航栏 `<img>` 与 `.title-logo`，同时移除 `gap`，使文字从原 Logo 左边界直接开始。
- 源码和当时现有的 `www/build` 已同步修改；S27-D继续等待重新生成 `fsdata.c`、编译烧录和实机复测。
- 无变化复测的产物审计确认路径没有指错：`www/src` 和 `www/build` 已在13:35至13:36更新，但 `fsdata.c`、httpd对象与 `build/GP2040-CE_0.0.0_Fightpad12Slim.uf2` 均停留在11:37，实际烧录固件不含删除图片修改。

### 讨论ID

`2026-07-31-webconfig-fightpad-branding`

## 2026-08-07: Key/Base 统一灯效与 Chase 加速规划 (S29)

### 需求与决策

- `RGB Customize` 将 `Key Effect` 与 `Base Effect` 合并为单一 `Light Effect`，统一提供 Static Color、Gradient、Breathing、Rainbow、Chase。
- GP22 Key 与 GP40 Base 共享一个运行时效果编号；Static/Breathing 同时写入两条链的颜色，动态效果每帧只推进一次共享相位。
- 普通 Chase 从 `200 ms/格` 改为 `160 ms/格`；蓝牙状态 Pairing/Connecting 的 `50 ms/格` Chase 不变。
- 保留 protobuf 的 `buttonEffectIndex` 与 `ambientEffectIndex` 字段和编号；启动时转换为统一效果，保存时映射回两个旧编号。
- 旧 Key/Base 配置不一致时优先采用 Key，Key 无有效值时采用 Base；旧颜色同样优先 Key。
- Key Flash、All OFF、GP24 门控、蓝牙 GP40 临时覆盖和 `SOC <= 7%` 低电最终关灯优先级不变。
- 按仓库约定不运行编译，由用户完成构建、烧录与实机验证。

### 讨论ID

`2026-08-07-unified-light-effect-chase-speed`

### 实现记录

- `RGB Customize` 已变为 Key Flash、Light Effect、Brightness、All OFF，All OFF 的命名索引同步从 4 调整为 3。
- 新增统一 `SWLightEffect` 与 `g_menuLightEffect`，并将两条运行时颜色状态合并为 `g_menuRgbEffectColor`。
- 启动时按 Key 优先规则读取旧效果/颜色；保存时将统一效果分别映射回合法的 `buttonEffectIndex` 与 `ambientEffectIndex`，protobuf 未修改。
- `FightpadAmbientLEDAddon` 在渲染入口单次更新共享 Gradient/Rainbow/Chase 相位，两条灯链不再各自推进动画。
- Static、Gradient、Breathing、Rainbow、Chase 均同步；Chase 共用动态色轮颜色，GP40 保留 5 灯对称梯度，GP22 保留 3 灯拖尾与 Key Flash 覆盖。
- 普通 Chase 使用 `FIGHTPAD12SLIM_LIGHT_CHASE_STEP_MS=160`；蓝牙状态分支仍为 `elapsedMs / 50U`。
- 旧分离状态残留搜索、菜单/映射/速度/优先级定向断言和 `git diff --check` 通过；按仓库约定未运行编译。
- S29-D 等待用户构建、烧录并验证五种效果、重启恢复、All OFF、蓝牙覆盖和低电保护。

## 2026-08-07: GP30 短按持久化开关普通灯效规划 (S30)

- 使用当前 ScrollWheelMenuAddon 五态状态机作为 GP30 短按/长按的唯一判定者，不恢复旧版 FightpadAmbientLEDAddon 独立读取 GP30 的实现。
- 菜单关闭时短按切换独立状态，只禁止普通 Light Effect 和 Key Flash，不修改菜单效果、颜色或亮度，并通过现有存储事件写入 Flash。
- 长按 2 秒进入/退出菜单以及菜单内短按确认保持不变。
- 手动关闭期间蓝牙 Pairing/Connecting/Connected 的现有 GP40 临时灯效和 GP24 临时供电保持不变；提示结束后重新熄灭。
- BQ27220 `SOC <= 7%` 低电保护继续保持最高优先级。
- 讨论ID: `2026-08-07-gp30-short-press-light-toggle`

### 实现记录

- 新增 `FightpadAmbientLEDOptions.manualLightEffectsEnabled` 字段和运行时 `g_manualLightEffectsEnabled`；旧配置缺少字段时默认开启。
- `navSelect()` 在菜单关闭时翻转该状态并触发现有 `GPStorageSaveEvent`；菜单打开时仍执行原有选择，`btnFromLong` 继续阻止长按释放调用 `navSelect()`。
- `FightpadAmbientLEDAddon` 的普通 `enabled` 改为菜单电源请求与手动状态相与；蓝牙 `bluetoothStatusLightRequired` 仍通过原有独立分支请求 GP40 和 GP24。
- 手动关闭时普通 GP22/GP40 与 Key Flash 熄灭；蓝牙提示可临时点亮 GP40，提示结束后重新熄灭。
- 菜单 `All OFF` 保持原来的颜色/效果覆盖式持久化行为，不改为新开关字段。
- 定向检查确认短按只在 `!btnFromLong` 时调用、菜单内仍执行原选择逻辑、蓝牙与低电最终输出表达式未被改写；`git diff --check` 通过。
- 按仓库约定未运行编译；S30-C 等待用户构建、烧录并实测短按恢复、长按菜单及蓝牙提示覆盖。
- 按用户最终决策，GP30 开关改为独立 protobuf 字段持久化；菜单 `All OFF` 继续使用原覆盖颜色和效果的实现。
- `ConfigUtils` 对旧配置将新字段默认初始化为 `true`；启动读取、短按翻转、`persistConfig()` 写回和存储事件调用链检查通过。
- 手动关闭使普通 `enabled` 为 false，因此蓝牙覆盖期间 `renderButtons()` 不执行，GP22 与 Key Flash 保持黑色；GP40 蓝牙状态和低电最终关灯表达式保持原样。
- 持久化修改后的 `git diff --check` 通过；仍未运行编译。

## 2026-08-07: 拨轮 Controller Type 上游模式接入 (S31)

- 在实体拨轮主菜单的 `RGB Customize` 后增加 `Controller Type`，不是新增 Web Config 页面。
- 模式固定为 XBOX、PS3、PS4、PS5、SWITCH、SWITCH PRO、KEYBOARD、GENERIC HID，并直接映射上游 `InputMode` 枚举。
- 不增加独立 Arcade；Arcade Stick 属于上游 `InputModeDeviceType`。Generic HID 保留上游名称，不包装成自定义 DInput 协议。
- 进入列表自动定位已保存模式，OLED 右侧 `*` 标记当前项，GP19 返回主菜单并恢复入口位置。
- 选择相同模式不写入、不重启；选择不同模式更新 `GamepadOptions.inputMode` 并触发 `GPStorageSaveEvent(true, true)`，保存后由 RP2350 重启完成 USB 重新枚举。
- 定向检查覆盖八项映射、菜单表/计数/显示/返回路径和保存重启调用；`git diff --check` 通过。
- 按仓库约定未运行编译；S31-D 等待用户构建、烧录和实机验证。

## 2026-08-07: 普通 Chase 加速至 100ms (S32)

- 将 `FIGHTPAD12SLIM_LIGHT_CHASE_STEP_MS` 的板级默认值由 160ms/格调整为 100ms/格。
- 普通 GP22 Key 与 GP40 Base 仍由同一共享 Chase 状态推进，未拆分动画相位。
- 蓝牙 Pairing/Connecting 的两个状态 Chase 分支继续使用 `elapsedMs / 50U`，速度和临时覆盖优先级不变。
- 定向搜索和 `git diff --check` 通过；按仓库约定未运行编译，等待用户烧录实测。

## 2026-08-07: 菜单非 OFF 灯效恢复 GP30 手动灯光 ON (S33)

- Key Flash 选择非 OFF 颜色、Static/Breathing 选择非 OFF 颜色，以及 Gradient/Rainbow/Chase 选择后，都会在保存前同步设置 `g_manualLightEffectsEnabled=true`。
- 颜色 `OFF`、菜单 `All OFF`、蓝牙临时覆盖和低电保护逻辑未改。
- 复用现有 `persistConfig()` 将效果、颜色和 GP30 手动状态一起写入 Flash，不新增 protobuf 字段。
- 控制器模式切换仍经过强制保存、500ms 延时和 RP2350 watchdog 硬件重启；LED 在供电未断时锁存最后一帧，因此视觉连续不代表软件假重启。
- 非 OFF 三条选择路径、OFF 隔离、保存顺序和 `git diff --check` 已通过；按仓库约定未运行编译，等待用户烧录验证。

## 2026-08-07: Controller Type 可见重启黑屏 (S34)

- 控制器模式实际变化时，在 `GPStorageSaveEvent(true, true)` 前置位 RAM-only `g_scrollWheelRebootBlackout`。
- FightpadAmbientLEDAddon 在普通 enabled、render 和最终 show 三层检查黑屏状态，确保 GP22/GP40 写入全黑且蓝牙临时灯效不能覆盖。
- 黑屏标志不进入 protobuf；watchdog 重启后自动恢复为 false，原 Flash 灯效重新加载。
- 相同模式不置位黑屏、不保存、不重启；现有 500ms 延时、watchdog 重启和 USB 重新枚举路径不变。
- 未改变当前 GP24 OFF 门控板级选项，避免扩大到其他灯光场景。
- 按仓库约定未运行编译，等待用户烧录观察约 500ms 熄灯提示和启动恢复。

## 2026-08-07: 普通 Light Effect 双段 Chase (S35)

- 普通 GP40 19 灯和 GP22 12 灯 Chase 均改为两个三灯拖尾段，第二段头灯为 `(head + count / 2) % count`。
- 两段亮度复用蓝牙双段 Chase 的 `0.80/0.25/0.05`，方向同为从头灯向后衰减。
- GP40 两段相隔9格，GP22相隔6格；两链继续共享 `lightChaseStep` 和动态色轮颜色。
- GP22 按键闪光覆盖保留；普通 Chase 仍为100ms/格，蓝牙状态 Chase 仍为50ms/格。
- 两链双段数量、半圈间隔、拖尾亮度、Key Flash、速度隔离、GP22最终写入所有权和 `git diff --check` 均通过。
- 按仓库约定未运行编译，等待用户烧录验证双段效果。

## 2026-08-07: GP22 1秒/圈与GP40 2秒/圈 (S36)

- 普通双段 Chase 不再让两条不同长度灯链共享同一个灯位步进。
- 新增 GP22=1000ms/圈、GP40=2000ms/圈板级常量，以 `(elapsed % cycleMs) * count / cycleMs` 按绝对经过时间计算头灯。
- GP22平均约83.3ms跨一灯，GP40平均约105.3ms跨一灯；20ms渲染调度不会累计拖慢整圈周期。
- 两链继续共享动态色轮颜色，并保留双段三灯拖尾、Key Flash和蓝牙50ms状态灯。
- 周期边界、旧共享步进清理、双段连接、蓝牙速度隔离和 `git diff --check` 均通过。
- 按仓库约定未运行编译，等待用户烧录计时验证。

## 2026-08-10: GP33传输选择、GP34 C6使能与消抖 (S37)

- 原理图只能确认GP33可切到3.3V或GND，不能仅凭图中文字确定装配后拨杆方向；首轮实机表现证明原推断相反，固件最终按低=USB、高=BT处理。
- GP34作为独立板级高有效EN输出：USB模式拉低，BT模式拉高；不复用WebConfig/DTR-RTS resetPin，GP35保持不驱动。
- GP34连接ESP32-C6 CHIP_PU/EN，因此低电平表示硬件关断/复位保持，不是ESP32软件light/deep sleep。
- 上电首次采样立即生效；运行中GP33候选电平必须连续稳定30ms才提交，使用无阻塞、无符号时间差实现。
- Core0 USB报告门控与ESP32 Proxy各自维护同参数消抖状态，避免跨核心共享可变状态；Proxy内部的UART输入帧、模式帧和GP34使用同一稳定状态。
- 同步修正文档中“GP34不驱动”的过期说明；按仓库约定不运行编译，等待用户构建烧录和实机验证。

## 2026-08-10: GP22 Chase 2秒周期与任意按键唤醒OLED (S38)

- 当前Fightpad板级OLED专用空闲休眠阈值为60000ms；该分支此前只消费GP30/31/32原子活动时间和蓝牙状态事件，且会在通用Display Saver按键逻辑之前提前关屏返回。
- GP22普通Chase周期从1000ms改为2000ms；GP40保持2000ms，普通颜色节拍保持100ms，蓝牙Pairing/Connecting的GP40 Chase保持50ms/颗。
- ScrollWheelMenuAddon在Core0读取现有已消抖GP2～GP20掩码；任意按键按下期间持续更新已有原子活动时间戳。
- Core1 DisplayAddon无需新增GPIO读取或跨核共享字段，继续从同一时间戳完成60秒休眠和立即唤醒。
- 唤醒路径不修改或吞掉游戏输入，不写Flash；GP30/31/32及蓝牙状态唤醒保持原样。
- 定向静态验证和`git diff --check`通过；按仓库约定未运行编译，等待用户构建烧录和实机复测。

## 2026-08-10: PS3残留设备类型容错 (S39)

- 实机对比为同一USB档位下Switch、PS4、PS5和Xbox输入正常，只有PS3能够枚举但无按键输入，因此排除共享按键扫描、传输选择和菜单输入锁。
- 根因路径是拨轮Controller Type只保存`inputMode`，会保留Web Config或PS5留下的`inputDeviceType=ARCADE_STICK`；PS3仅在严格等于`GAMEPAD`时生成标准报告，其他值进入备用报告路径。
- 拨轮菜单现在把八种普通控制器选择统一保存为`GAMEPAD`，并在当前模式相同但子类型错误时仍强制保存和重启。
- PS3驱动初始化对白名单之外的Arcade Stick、HOTAS和Mecha类型直接回退为标准Gamepad，保证烧录后即使Flash仍有旧值也能立即恢复输入。
- 保留PS3官方支持的Gamepad Alternate、Wheel、Guitar和Drum；不修改Switch、PS4、PS5、Xbox驱动与Web Config专业配置能力。
- 按仓库约定不运行编译；定向源代码检查和`git diff --check`完成后由用户构建、烧录并验证。
- S39-D首轮实机失败：对象文件、ELF和UF2时间戳均晚于源码，确认新修复已进入固件；USB PS3仍能枚举但无按键输入，而蓝牙和其他USB模式正常。问题继续收敛到PS3专用描述符、报告提交或PC主机接收链路，记录于`HVR-S39-001`。
- Windows设备层确认`VID_054C&PID_0268`、微软`HidUsb`和HID游戏控制器子设备均正常；绕过网页直接监听20秒并持续按键，输入报告数仍为0，排除测试网站和驱动异常。
- PS3 HID描述符声明48字节数据加Report ID共49字节，原代码却以51字节`sizeof(PS3Report)`提交；已将中断IN发送与控制`GET_REPORT`统一限制为49字节，等待用户自行构建烧录复测。

## 2026-08-11: 启动画面保护与绑定设备蓝牙准入规划 (S40)

- RP2350 的 3 秒 Splash 期间继续接收蓝牙 UART 状态，但不渲染 OLED 蓝牙覆盖页；持续状态可在 Splash 后显示，已过期的 Connected/Disconnected 不补播。
- ESP32-C6 取消开机自动普通配对窗口；存在绑定时先高占空比、再低占空比定向广播到最近绑定身份地址。
- 无绑定且未按 GPIO13 时保持不广播；只有 GPIO13 的 30 秒显式窗口允许普通广播和新绑定。
- 连接建立时记录是否来自显式配对窗口；非显式连接必须来自定向广播，Repeat Pairing 仅对显式配对连接开放。
- 不改变 GPIO、UART 协议、GP33/GP34 传输使能、蓝牙状态灯或配对窗口时长。
- 按仓库约定只做静态验证，由用户完成双端构建、烧录和实机回归。

### 讨论ID

`2026-08-11-splash-bonded-peer-guard`

### 实现记录

- 权威 ESP32-C6 工程确认是 `E:\WorkSpace\C_WorkSpacee\ESP-IDF5.2\.espressif\release-v5.2\esp32c6_ble_hid_gamepad_test`；误改的 `E:\ComporyProject\aa\esp32c6_ble_hid_gamepad_test\main\main.c` 已精确恢复到本轮修改前内容（Git blob `fdd1ccb95789d2c860242a055f6037d01677ed01`）。
- `DisplayAddon` 在 `DisplayMode::SPLASH` 时继续消费和计时蓝牙事件，但跳过蓝牙文字覆盖；Pairing/Connecting 若在 3 秒后仍有效可正常出现，Connected/Disconnected 的 1 秒结果不会延迟补播。
- ESP32-C6 启动路径不再调用 `open_pairing_window(false)`；恢复最近绑定身份时排队一次高占空比定向突发，结束后保持 200ms 低占空比定向广播。
- 无绑定且无 GPIO13 窗口时 `manage_advertising()` 主动保持 `ADV_MODE_OFF`；GPIO13 窗口仍使用 30-50ms 普通广播。
- 每次连接记录 `connection_authorized`、`connection_allows_new_bond` 和最近广播类型；未授权连接不能进入 HID 订阅/报告路径，并会请求断开。
- 未授权连接不能改写最近绑定身份；未授权加密链接和 Repeat Pairing 会被拒绝。
- 同步更新 C6 蓝牙架构、功耗说明和 Fightpad12Slim OLED 行为说明；双端 `git diff --check` 通过，未执行构建或烧录。
- S40-D 等待用户构建烧录并验证完整 3 秒 Logo、旧主机定向回连、无绑定静默和 GPIO13 新配对。

## 2026-08-12: USB独立的多类型BLE控制器Profile规划 (S41)

- 完成五阶段需求讨论，选择“ESP32-C6单固件、多套启动时选择的BLE Profile”，支持 Xbox、Generic、Keyboard 和实验性 PS5-PC。
- RP2350作为产品级Profile真值来源，使用独立持久化字段，不复用或修改USB `InputMode`。
- 跨芯片协议v1保持现有8字节帧和XOR校验，新增RP到C6的`FM` Mode帧及C6到RP的`FA` ACK帧，并使用sequence拒绝旧应答。
- Profile实际变化时C6持久化pending状态、回复ACK并独立重启；启动时清理不兼容绑定并开启30秒配对。同Profile普通开机只重连，不重新配对。
- 正式交接文档为`docs/ESP32C6_BLE_PROFILE_HANDOFF.md`；当前阶段先实现RP2350配置、菜单、UART状态机和OLED提示，C6由权威工程另行配套实现。

### RP2350实现记录

- `FightpadESP32ProxyOptions`新增field 12 `bluetoothProfile`，默认Xbox；旧配置缺少字段时由proto默认值迁移，不复用USB `InputMode`。
- 拨轮主菜单新增`Bluetooth Type`及Xbox/Generic/Keyboard/PS5-PC四项；USB档位只保存，BT档位由Proxy轮询配置后同步，不触发RP2350重启。
- 新增公共BLE Profile枚举和协议常量；RP发送`FM` v1帧，250ms重试、2秒超时，C6的`FA` ACK必须匹配version、sequence和accepted profile。
- 超时或协议错误会阻止相同配置的无限重试，直到Profile改变或USB/BT重新切换；`RESTARTING/APPLYING_AT_BOOT`显示`Pair Again`，随后由现有`FS`状态显示Pairing。
- OLED Profile覆盖页沿用现有蓝牙弹窗和Splash抑制路径，不重新加载旧画面；`Ready`事件会立即清除Applying覆盖。
- 协作文档中的protobuf字段号已从规划占位值11纠正为实际未占用的12，与源码一致。
- 已完成菜单分支、Parser允许类型、字段号一致性、协议路径和`git diff --check`非编译静态检查；按仓库约定未运行构建，S41-E等待C6实现及用户双端构建烧录联调。
- S41联调发现多Profile改造后GPIO13按键不再稳定显示Pairing。RP端`FS 03`解析、OLED与灯效路径完整，物理按键直接属于C6，因此未给RP增加无关补丁。
- C6权威工程当前`update_ble_status_output()`把`hid_connected`放在`pairing_status_active()`之前，已连接时会遮住用户主动打开的Pairing；`trigger_pairing_mode()`也应立即发送`FS 03`并可靠请求断开旧链路。
- 已新增`docs/ESP32C6_GPIO13_PAIRING_REGRESSION_HANDOFF.md`，按“按键边沿日志→Transport→状态优先级→断链→快速广播”的顺序交给C6 AI修复；GPIO13电平仅在边沿日志缺失且实测确认后调整。
- `BLE_PROFILE_FLAG_FORCE_REPAIR`在C6当前仍是no-op，文档要求补完或明确保留TODO；RP当前只发送`APPLY_NOW`，该flag不是物理按键无响应的直接原因。

## 2026-08-13: 当前传输控制器类型统一显示 (S42)

- 主页面不再始终使用RP2350 USB InputMode作为控制器类型来源。
- Proxy将现有GP33 30ms消抖结果与C6 `FA` ACK确认Profile组合为线程安全跨核快照；
  BT挡位且ACK有效时才向Core1返回BLE Profile。
- 切入BT时先使旧确认Profile失效；ACK回来前主页面回退到USB标签，避免把尚未生效的
  菜单选择显示成当前模式。Profile切换期间保留旧已生效模式，收到新ACK后再更新。
- BUTTONS主页面BLE映射：Xbox=`XINPUT`、PS5=`PS5`、Generic=`USBHID`、
  Keyboard=`HID-KB`。根据用户返工意见，USB XInput/XBOne/PS5恢复并保留上游原始
  `XINPUT`/`XB360`、`XBON*`/`XBONE`和认证状态显示，不为视觉命名改动既有逻辑。
- 输入历史的A/B/X/Y与PlayStation符号同样按当前有效传输类型选择。
- 未新增UART帧、未改USB/BLE配置、未新增RP2350重启；定向`git diff --check`通过。
- 按仓库约定未运行RP2350编译，等待用户构建、烧录和实机验证。

### 讨论ID

`2026-08-13-active-transport-controller-label`

## 2026-08-13: BLE Profile强制持久化与同步门控 (S43)

- C6侧协作任务书确认实机故障链：PS5 USB认证环境下菜单原用`GPStorageSaveEvent(false)`，会被`Storage::save(false)`保护拒绝，导致Xbox只存在RAM；重启后旧PS从Flash回灌C6并触发真实Profile变化和重新配对。
- Bluetooth Type改为`GPStorageSaveEvent(true, false)`：只对此用户明确配置强制写Flash，不重启RP2350，也不修改全局Storage保存策略。
- 菜单仍同时设置`has_bluetoothProfile=true`和`bluetoothProfile`；Proxy启动读取增加`has_`检查，缺失时使用独立Xbox默认值，不读取USB`GamepadOptions.inputMode`。
- 新增Core0保存进行中门控；Proxy在Flash结果未知时不发送RAM目标值。保存成功后自动继续现有Mode同步，失败时恢复旧`has_`和值、记录一次日志并在OLED显示`Save Failed`。
- `GP2040::checkSaveRebootState()`开始保存并使用`Storage::save()`返回值，但只有存在BLE Profile pending时才执行BLE回滚/提示，其他保存行为不变。
- 增加菜单选择、保存请求/结果、启动配置、首次Mode TX和ACK的单次日志；没有放入主循环刷屏路径。
- 普通Mode帧继续固定`APPLY_NOW=0x01`，未使用`FORCE_REPAIR`。Xbox示例`46 4D 01 01 01 01 00 0B`的XOR校验为`0B`。
- 修改范围的`git diff --check`通过；按仓库约定未运行GP2040-CE构建或烧录，S43-D等待用户实机验证。

### 讨论ID

`2026-08-13-ble-profile-persistence`

## 2026-08-17: Switch BLE Profile 4 RP2350协同 (S44)

- `docs/ESP32C6_SWITCH_BLE_PROFILE_RP2350_HANDOFF.md` 已覆盖需求、协议、硬件边界、风险和联调步骤，作为本轮已确认讨论依据。
- 保持 Generic=0、Xbox=1、Keyboard=2、PS5PC=3 不变，只在末尾追加 Switch=4；Profile 3当前标签为PS4兼容BLE。
- `isValidFightpadBluetoothProfile()` 扩展到0..4，菜单通过`targetIndex`显式发送4；protobuf字段号、默认值和存储类型不变。
- Proxy继续复用现有Mode/ACK、sequence、250ms重试、2秒超时、强制保存成功后同步和失败回滚路径，没有新增Switch专属状态机。
- 补齐主页面BLE显示：Profile 3显示PS4布局，Profile 4显示SWITCH并使用Switch按钮布局；有线USB InputMode和驱动未修改。
- 主协议文档同步Profile 0..4、Switch固定测试向量、多Profile独立Bond和回归场景。
- 按仓库约定未运行编译；S44-D等待用户构建、烧录并完成双固件实机联调。

### 讨论ID

`2026-08-17-switch-ble-profile-4`

## 2026-08-17: BLE控制器选择后返回主页面 (S45)

- Bluetooth Type列表短按确认后不再停留在选择界面，统一调用现有`navToggle()`退出菜单并恢复BUTTONS页面。
- Profile值写入RAM、强制Flash保存、保存成功后Proxy同步、C6 Mode/ACK和重启流程保持原样且继续异步执行。
- Applying、Pairing和Save Failed仍通过现有OLED临时覆盖层显示；无需让菜单等待保存或ACK。
- 选择当前相同Profile也会退出列表，符合“完成一次选择即返回主页面”的交互行为。
- 按仓库约定未运行编译；S45-B等待用户构建烧录并验证菜单输入锁释放和提示显示。

### 讨论ID

`2026-08-17-ble-profile-return-buttons`

## 2026-08-17: BLE控制器菜单精简 (S46)

- Bluetooth Type菜单删除Keyboard入口，保留Xbox、Generic、PS和Switch四个量产选项；菜单计数继续由数组大小自动计算。
- Profile 2 Keyboard仍保留在公共枚举、合法范围、UART Mode/ACK和配置读取中，避免已保存数值2失效。
- Profile 3内部枚举`PS5PC`和值3保持不变，菜单与Profile切换提示统一显示`PS BLE`。
- BUTTONS主页面的紧凑当前类型仍显示`PS4`，本轮只修改用户指定的BLE Controller菜单及其切换提示。
- 上一次构建在用户中断后不视为有效产物；本轮按仓库约定未重新编译。

### 讨论ID

`2026-08-17-ble-menu-trim`

## 2026-08-17: BLE控制器切换可见重启 (S47)

- Bluetooth Type实际变化时将保存事件从`GPStorageSaveEvent(true, false)`改为`GPStorageSaveEvent(true, true)`，与USB Controller Type一致。
- 保存请求前置位RAM-only `g_scrollWheelRebootBlackout`，两条灯链立即写入黑帧；约500ms后RP2350 watchdog重启并显示现有启动Logo。
- `checkSaveRebootState()`先执行`Storage::save()`和BLE保存结果处理，再启动重启倒计时，确保Flash提交先于重启。
- 保存成功后Proxy可在500ms窗口内同步C6；即使尚未完成，RP2350新启动也会从Flash读取目标Profile并重新同步。
- 选择当前相同Profile不会创建保存事件、不会置位黑屏也不会重启，只退出列表返回BUTTONS页面。
- UART版本、Mode/ACK格式、Profile编号、C6重启职责和Bond隔离不变；本轮未运行编译。

### 讨论ID

`2026-08-17-ble-profile-visible-reboot`

## 2026-08-18: GPIO13配对状态幂等切换 (S49)

- 以ESP32-C6协作文档为既定协议依据：`FS 03`进入并持续显示Pairing，`FS 00/01/02`任一合法状态都立即退出Pairing，不新增取消状态或修改8字节UART格式。
- 审计确认OLED和GP40都消费统一的`bluetoothStatusSnapshot`，不存在页面栈；原问题是`publishBluetoothStatus()`对每次重发都更新时间戳并递增序号，导致重复唤醒和Pairing动画重置。
- 在统一发布点增加相同状态幂等去重：快照有效且状态未变化时直接返回，保留原`receivedAtMs`和`sequence`；不同状态仍立即发布并替换当前Pairing。
- 该修改不碰Profile编号、名称、VID/PID、Bond、校验、ACK、重启流程或ESP32-C6工程。
- 完成`03→00`、`03→01`、`03→02`及重复`03`路径的非编译静态检查；按协作文档约定未运行构建和烧录，S49-D由用户完成。

### 讨论ID

`2026-08-18-gpio13-pairing-toggle-rp2350`

## 2026-08-18: Web Config Button预览B1直接返回 (S50)

- 用户确认Button预览仍复用正常开机的`ButtonLayoutScreen`，但Web Config下B1只作为返回键，不显示B1按键动画；不改用A2。
- `ButtonLayoutScreen::update()`在`configMode`下改为检测B1按下边沿，并在布局、输入历史和绘制前请求返回`CONFIG_INSTRUCTION`；正常游戏模式以及其他按键预览保持不变。
- `DisplayAddon::process()`对该特定转换先切换并绘制新页面，不再绘制一次旧Button页，因此B1不会闪现按下图案。
- 返回后启用输入释放排空，在B1及同时按住的其他按钮全部松开前不把输入交给`ConfigScreen`，避免同一次B1松开再次进入Button页面。
- 完成条件分支、转换顺序和正常模式隔离的非编译静态检查；按仓库约定未运行构建，等待用户烧录验证。
- 首次实机复测仍在B1退出时卡住；UF2时间16:19晚于相关源码16:15，确认不是旧固件。
- 根因定位到`EventManager::unregisterEventHandler()`：遍历回调向量时错误递增外层`it`而不是`funcIt`，Button页退出调用`shutdown()`注销事件时会卡在同一个回调或破坏外层迭代器。
- 将循环增量修正为`funcIt++`；同时把释放门控收回`ConfigScreen`本地，页面由B1按下边沿切换且在旧Button页绘制前完成，保持B1无动画。

### 讨论ID

`2026-08-18-webconfig-button-b1-return`

## 2026-08-18: Custom Theme 上移到 Lighting 菜单 (S51)

- 用户指出Custom Theme同时提供Normal和Pressed颜色，作为Lighting Effect子项会让Button Flash状态语义不一致。
- 将其从`kMenuLightEffects`移到`kMenuRgbSub`，菜单顺序改为Button Flash、Lighting Effect、Custom Theme、Brightness、Turn Lights Off。
- Custom Theme运行时只在父级Custom Theme行显示`*`；Lighting Effect和Button Flash子页不显示被覆盖的备用状态，但不清除备用配置。
- 未定义提示的超时、短按关闭和Back全部返回Lighting父级Custom Theme行；运行时编号5、兼容存储值7、Web Config开关与延迟停用规则保持不变。
- 完成菜单计数、All Off索引、选择路径、状态标记和持久化编号的非编译静态检查；构建与烧录由用户完成。

### 讨论ID

`2026-08-18-custom-theme-parent-menu`

## 2026-08-18: Web Config重启页复用配置启动图 (S52)

- 审计确认Web Config重启API触发`GPRestartEvent`并进入独立`RestartScreen`；旧页面固定绘制`bitmapGP2040Logo`，与正常开机读取Flash `splashImage`的`SplashScreen`不是同一路径。
- 将RestartScreen图片源改为`getDisplayOptions().splashImage.bytes`，复用128×64、pitch=16参数，因此量产默认图和Web Config上传图都能自动同步到重启页面。
- 完整图片绘制后清除底部两行作为文字区，保留Controller/WebConfig/BOOTSEL现有模式说明和`Please Wait`等文字。
- 删除RestartScreen对旧`BitmapScreens.h` Logo的依赖，不修改启动图存储、Splash时长、Web API、GPRestartEvent或重启流程。
- 完成图片源、尺寸、文字分支和旧Logo引用的非编译静态检查；构建与烧录由用户完成。

### 讨论ID

`2026-08-18-restart-screen-configured-splash`

## 2026-08-19: 蓝牙档位与USB HID互斥 (S53)

- 审计确认旧逻辑在GP33蓝牙档位仍保持RP2350 USB控制器枚举，只持续发送中立报告；这会让PC同时看到BLE控制器和一个无输入的USB控制器，存在游戏选错设备的量产体验风险。
- 在`src/gp2040.cpp`增加TinyUSB连接状态机：普通控制器进入蓝牙档位时调用`tud_disconnect()`，切回USB档位时调用`tud_connect()`重新枚举；USB线仍可承担硬件供电和充电。
- USB转蓝牙时沿用原中立报告路径，并设置20 ms有界等待；成功发出中立报告、主机未挂载或等待到期后均会断开，避免端点繁忙导致USB设备永久残留。
- 同一轮循环只读取一次经30 ms消抖的GP33状态，同时驱动USB报告路由和USB连接状态，避免切换瞬间两条路径采用不同档位。
- Web Config初始化时无条件保持TinyUSB连接；BOOTSEL在`run()`之前进入ROM，不受新状态机影响。未修改BLE Profile、USB InputMode、UART协议、Flash配置或ESP32-C6工程。
- 完成TinyUSB RP2350 DCD拉起/撤销上拉实现、调用范围、配置模式边界和工作树差异的非编译静态检查；构建、烧录和实机验证由用户完成。

### 讨论ID

`2026-08-19-bt-usb-hid-exclusive`

## 2026-08-19: 控制器菜单按当前传输档位限制 (S54)

- 审计确认主菜单`USB Mode`和`Bluetooth Mode`此前无GP33入口门控，两种物理档位都能进入并修改另一传输通道的控制器类型。
- 在ESP32 Proxy现有临界区快照上增加只读查询接口，菜单使用已经过30 ms消抖并发布的传输状态，不重复实现GP33判定和消抖。
- 根据后续要求将提示门控改为入口过滤，两张等长菜单表保持公共项和父级索引兼容；入口文案随后统一为美式游戏界面标题`Controller Mode`，USB档进入有线列表，蓝牙档进入蓝牙列表。
- 菜单打开期间检测已消抖传输快照变化并刷新显示；若当时停留在失效的控制器子菜单，则自动返回主菜单并选中新档位入口。
- 正确档位的类型列表、USB InputMode保存重启、BLE Profile保存重启逻辑均未修改；不修改GP33/GP34极性、协议和配置格式。
- 完成残留提示符号、对称菜单表、跨核状态、索引路径和差异静态检查；复用`build-ce-no-picotool`完成增量构建并用picotool验证RP2350 UF2，烧录与实机验证待用户完成。

### 讨论ID

`2026-08-19-transport-aware-controller-menus`

## 2026-08-19: Turn Lights Off可恢复总开关 (S55)

- 审计确认旧`Turn Lights Off`会把三个颜色写成黑色、两个灯效写成0xFF并保存，原配置被销毁，第二次点击无法恢复；蓝牙状态灯还可绕过该关闭状态临时点亮GP40。
- 在`FightpadAmbientLEDOptions`增加字段8 `allLightsEnabled`，旧配置缺少该字段时默认开启；菜单保存总开关时不再修改颜色、效果、Button Flash、亮度或GP30普通灯效开关。
- 菜单项根据当前总开关动态显示`Turn Lights Off`或`Turn Lights On`；再次选择恢复关闭前的完整配置，断电重启后开关状态保持。
- 总开关在`render()`蓝牙状态分支之前和`show()`最终供电判断中双层门控，确保GP22、GP40、Button Flash、蓝牙配对/连接状态灯和GP24均关闭；GP30仍只控制普通灯效。
- 旧版全黑/0xFF破坏性关闭编码无法还原历史原值，迁移时保持关闭并在RAM准备静态白色，首次Turn Lights On后保存为安全可见配置。
- 更新Fightpad板级README和RGB子系统文档；完成字段编号、默认初始化、菜单索引、非破坏性保存、渲染与GP24路径的非编译静态检查，构建烧录由用户完成。

### 讨论ID

`2026-08-19-persistent-all-lights-toggle`

## 2026-08-19: FightpadSlim USB产品名 (S56)

- 截图中的`GP2040-CE (PS4)`来自PS4驱动USB Product String，不是OLED菜单文字；该模式使用VID/PID `1532:0401`。
- 在TinyUSB全局字符串回调中只对`GP2040_BOARDCONFIG="Fightpad12Slim"`和产品字符串索引2返回`FIGHTPADSLIM`，因此所有有线控制器模式统一命名，其他板型和非产品字符串继续交给原驱动。
- 未修改VID/PID、PS4/PS3/XInput等协议描述符、USB InputMode存储或ESP32-C6蓝牙广播名称。
- 复用`build-ce-no-picotool`完整构建成功；最终ELF反汇编显示描述符长度/类型`0x031A`及UTF-16字符序列`FIGHTPADSLIM`，picotool确认产物仍为RP2350 ARM Secure、Fightpad12Slim、SDK 2.2.0。

### 讨论ID

`2026-08-19-fightpadslim-usb-product-name`

## 2026-08-19: USB常亮与蓝牙统一省电 (S57)

- 复用现有`g_scrollWheelLastActivityMs`和60秒OLED超时，不增加持久化字段；共享查询只在GP33已消抖快照为BT且活动超时时返回休眠。
- USB档在OLED电源判断中保持开启并绕过通用Display Saver；快照尚未发布时按USB安全回退，避免启动阶段误关屏。
- BT档超时后，RGB在`render()`清空两条帧并提前返回，`show()`最终写入门控再次拦截，发送黑帧后拉低GP24；灯效、颜色、Button Flash、GP30和Turn Lights Off配置均不改变。
- GP2-GP20、GP30、GP31、GP32继续更新共享活动时间；新增GP33传输变化和新的蓝牙状态/Profile事件刷新时间，按键唤醒后下一帧恢复OLED和原RGB效果。
- 复用`build-ce-no-picotool`构建成功；低电保护、控制器重启黑屏和持久化总开关仍保持更高优先级，烧录实机验证待用户完成。

### 讨论ID

`2026-08-19-transport-aware-display-rgb-sleep`

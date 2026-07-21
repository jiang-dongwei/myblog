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

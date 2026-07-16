# 项目规格单

## 项目信息

| 项目 | 值 |
|------|-----|
| 名称 | Fightpad12Slim RP2350B Firmware |
| 基于 | GP2040-CE v0.7.12 |
| 开发板 | RP2350B (SparkFun Pro Micro RP2350) |
| BoardConfig | Fightpad12Slim |
| 编译目标 | `build/` (rp2350-arm-s) |

## 新增功能模块

### 拨轮开关菜单系统
- 物理硬件: 旋转编码器 (GP30=SW, GP31=A, GP32=B)
- 功能: 长按GP30进入→OLED显示多级菜单→拨轮导航→长按退出
- 讨论ID: `2026-07-06-scrollwheel-menu`

### Chase 动态变色修复
- 物理硬件: 无新增硬件，复用 GP22 按键灯链与 GP40 环境灯链
- 功能: RGB Customize 中 Chase 模式恢复动态变色，避免被 White/static color 覆盖
- 讨论ID: `2026-07-09-chase-color-cycle-fix`

### Base Chase 平滑亮度梯度
- 物理硬件: 无新增硬件，复用 GP40 环境灯链
- 功能: Base Effect 的 Chase 使用首尾暗、中间亮的 5 灯梯度，提升追逐流畅度
- 讨论ID: `2026-07-09-base-chase-smooth-gradient`

### Static Color 与 Breathing 拆分
- 物理硬件: 无新增硬件，复用 GP22 按键灯链与 GP40 环境灯链
- 功能: Base/Key 的 Static Color 改为固定选中颜色与固定亮度；Key Effect 新增 Breathing 选项承接原 Static Color 呼吸效果
- 讨论ID: `2026-07-09-static-color-breathing-split`

### Breathing Rainbow 与 Breathing Color 菜单拆分
- 物理硬件: 无新增硬件，复用 GP22 按键灯链与 GP40 环境灯链
- 功能: Base Effect 与 Key Effect 都提供自动变色呼吸 `Breathing Rainbow` 和可选单色呼吸 `Breathing Color`
- 讨论ID: `2026-07-09-breathing-rainbow-color-menu`

### RGB Effect 菜单精简
- 物理硬件: 无新增硬件，复用 GP22 按键灯链与 GP40 环境灯链
- 功能: Key Effect 删除 Static Theme，Key/Base 的 Breathing 都进入颜色菜单做单色呼吸或 OFF；Base Effect 删除自动变色 Breathing，Static Theme 改名 Rainbow
- 讨论ID: `2026-07-09-rgb-effect-menu-trim`

### BQ27220 电量 SOC/FCC 稳定性
- 物理硬件: BQ27220 电量计 (GP25=SCL, GP26=SDA, GP27=GPOUT)，BQ27220 直接电池供电
- 功能: 审计 BQ27220 配置写入与 FCC/SOC 跳变，使用现有 OLED 诊断页人工复测低电重启与 FCC 恢复现象
- 讨论ID: `2026-07-09-bq27220-battery-soc-fcc-stability`

### RP2350B 固件信息菜单
- 物理硬件: RP2350B 主控与现有 128x64 OLED，无新增引脚
- 功能: 在拨轮菜单的 RP2350B INFO 页显示 Pico SDK 版本、平台、板级配置和 CPU 架构
- 讨论ID: `2026-07-10-rp2350-firmware-info-menu`

### ESP32-C6 固件信息串口接收与菜单显示
- 物理硬件: RP2350B UART0 GP44/GP45 与 ESP32-C6 UART0 GPIO17/GPIO16，115200 8N1
- 功能: 接收 ESP32-C6 多帧固件信息，解析 SDK/Plat/Board/CPU，并在 ESP32 INFO 页显示；无数据时显示 `Coming to soon`
- 讨论ID: `2026-07-13-esp32-firmware-info-uart`

### Key Effect Gradient
- 物理硬件: 无新增硬件，复用 GP22 的 12 颗按键 WS2812 LED 链
- 功能: 在 RGB Custom 的 Key Effect 菜单增加 Gradient，使用独立色轮状态实现全按键同步变色并保留 Key Flash
- 讨论ID: `2026-07-13-key-effect-gradient`

### BQ27220 启动配置回读与电流校准
- 物理硬件: BQ27220 (GP25/GP26/GP27)、10 mOhm 采样电阻、318 mA 校准负载与外部电流表
- 功能: 扩展启动实际回读与选择性修复，在层级 0 的 Battery Info 四页菜单显示运行数据、EDV/Battery 配置、CC 校准和充电终止状态，并在实测后固化 CC Gain/CC Delta
- 讨论ID: `2026-07-13-bq27220-config-readback-calibration`

### BQ27220 电量档位串口快照（历史模式，已由 S18 周期模式替代）
- 物理硬件: RP2350B UART1，GP42=TX、GP43=RX，115200 8N1
- 功能: 最初在 SOC 精确到达 100/75/50/25/15/10/7/3/0% 时打印 P1-P4；S18-A 已改为每次 2 秒轮询后打印
- 讨论ID: `2026-07-14-bq27220-battery-uart-logging`

### 7% 低电灯光关闭
- 物理硬件: 复用 BQ27220、GP22 的 12 颗按键灯和 GP40 的 19 颗环境灯
- 功能: 有效 SOC 小于等于 7% 时强制全部灯光关闭，回升到 8% 后恢复当前菜单灯效且不改持久化配置
- 讨论ID: `2026-07-14-low-battery-led-cutoff`

### 电量周期串口、OLED休眠与主页面电量格
- 物理硬件: BQ27220、UART1 GP42/GP43、SSD1306 OLED、GP30/GP31/GP32
- 功能: 每次 2 秒 BQ 轮询后输出 P1-P4；GP30/31/32 无操作 60 秒后 OLED 关屏并由首次输入唤醒；BUTTONS 右上角用四格图标表示 SOC、右下角保留百分比，不显示 V/I/FCC 诊断数值
- 讨论ID: `2026-07-15-battery-uart-oled-sleep-icon`

### 隐藏 Battery Info 菜单入口
- 物理硬件: 无变化
- 功能: Fightpad12Slim 层级 0 隐藏 Battery Info，但保留四页诊断和导航实现，通过板级宏可随时恢复
- 讨论ID: `2026-07-15-hide-battery-info-menu`

### RGB Customize 三档亮度控制
- 物理硬件: 无新增硬件，复用 GP22 的 12 颗按键灯与 GP40 的 19 颗环境灯
- 功能: RGB Customize 增加 Brightness，可用 Bright/Normal/Dim 三档同时控制 Key/Base 的 Static Color、Gradient、Rainbow，并持久化当前档位
- 讨论ID: `2026-07-15-rgb-brightness-levels`

## 开发步骤状态

| 步骤 | 描述 | 状态 | 讨论ID |
|------|------|------|--------|
| S1-A | 拨轮编码器输入驱动 | pending | 2026-07-06-scrollwheel-menu |
| S1-B | 菜单数据模型 + OLED渲染 | pending | 2026-07-06-scrollwheel-menu |
| S1-C | FightpadAmbientLEDAddon GPIO仲裁 | pending | 2026-07-06-scrollwheel-menu |
| S1-D | 模式管理器 + ScrollWheelMenuAddon | pending | 2026-07-06-scrollwheel-menu |
| S1-E | 编译验证 + 固件烧录 | pending | 2026-07-06-scrollwheel-menu |
| S2-A | 修复长按GP30时LED误切换 (删除 g_scrollWheelButtonBusy 抑制) | completed | 2026-07-07-longpress-no-led-toggle |
| S3-A | INFO页禁用拨轮滚动 + COLOR层级短按返回上层 | completed | 2026-07-07-menu-nav-fixes |
| S4-A | 全局颜色状态变量 + COLOR层级短按写入颜色 | completed | 2026-07-07-rgb-color-control |
| S4-B | render()使用菜单颜色覆盖 + 按钮闪灯颜色 | completed | 2026-07-07-rgb-color-control |
| S5-A | RGB_SUB增加"RGB OFF" + COLOR增加"OFF"色 | completed | 2026-07-07-rgb-off |
| S6-A | Chase菜单选择时清空颜色覆盖 | completed | 2026-07-09-chase-color-cycle-fix |
| S6-B | 渲染层按effect类型选择颜色源 | completed | 2026-07-09-chase-color-cycle-fix |
| S6-C | Chase/Static Color/RGB OFF回归验证 | pending | 2026-07-09-chase-color-cycle-fix |
| S7-A | Base Chase亮度梯度调整 | completed | 2026-07-09-base-chase-smooth-gradient |
| S7-B | Button Chase前暗后亮梯度与其他RGB模式隔离确认 | completed | 2026-07-09-base-chase-smooth-gradient |
| S8-A | Base/Key Static Color固定亮度渲染 | completed | 2026-07-09-static-color-breathing-split |
| S8-B | Key Effect新增Breathing并迁移呼吸逻辑 | completed | 2026-07-09-static-color-breathing-split |
| S8-C | Static/Breathing/Chase/Rainbow回归验证 | pending | 2026-07-09-static-color-breathing-split |
| S9-A | Base/Key Effect菜单拆分Breathing Rainbow与Breathing Color | completed | 2026-07-09-breathing-rainbow-color-menu |
| S9-B | Base/Key Breathing Color颜色选择层级与渲染 | completed | 2026-07-09-breathing-rainbow-color-menu |
| S9-C | Breathing Rainbow/Breathing Color实机验证 | pending | 2026-07-09-breathing-rainbow-color-menu |
| S10-A | Key Effect删除Static Theme并将Breathing改为单色呼吸颜色菜单 | completed | 2026-07-09-rgb-effect-menu-trim |
| S10-B | Base Effect删除自动变色Breathing并将Static Theme改名Rainbow | completed | 2026-07-09-rgb-effect-menu-trim |
| S10-C | Key/Base菜单项与选色行为实机验证 | pending | 2026-07-09-rgb-effect-menu-trim |
| S11-A | BQ27220配置写入行为审计 | completed | 2026-07-09-bq27220-battery-soc-fcc-stability |
| S11-B | BQ27220诊断读取补强 | completed | 2026-07-09-bq27220-battery-soc-fcc-stability |
| S11-C | SOC/FCC跳变实机复测 | pending | 2026-07-09-bq27220-battery-soc-fcc-stability |
| S11-D | 低电灯效侧降耗策略评估 | pending | 2026-07-09-bq27220-battery-soc-fcc-stability |
| S12-A | RP2350B固件信息菜单页 | completed | 2026-07-10-rp2350-firmware-info-menu |
| S12-B | 固件信息页静态检查 | completed | 2026-07-10-rp2350-firmware-info-menu |
| S12-C | 固件信息页实机验证 | pending | 2026-07-10-rp2350-firmware-info-menu |
| S13-A | UART RX与固件信息帧状态机 | completed | 2026-07-13-esp32-firmware-info-uart |
| S13-B | Payload解析与跨核快照 | completed | 2026-07-13-esp32-firmware-info-uart |
| S13-C | ESP32固件信息菜单页 | completed | 2026-07-13-esp32-firmware-info-uart |
| S13-D | 固件信息静态与实机验证 | pending | 2026-07-13-esp32-firmware-info-uart |
| S14-A | Key Effect菜单增加Gradient并保持效果编号兼容 | completed | 2026-07-13-key-effect-gradient |
| S14-B | GP22 Key Gradient独立动画状态与Key Flash覆盖 | completed | 2026-07-13-key-effect-gradient |
| S14-C | Key Gradient代码静态验证 | completed | 2026-07-13-key-effect-gradient |
| S14-D | Key Gradient菜单与灯效实机验证 | pending | 2026-07-13-key-effect-gradient |
| S15-A | BQ27220启动配置完整回读 | completed | 2026-07-13-bq27220-config-readback-calibration |
| S15-B | Battery Low与EDV选择性修复 | completed | 2026-07-13-bq27220-config-readback-calibration |
| S15-C | Battery Info菜单与四页OLED显示 | completed | 2026-07-13-bq27220-config-readback-calibration |
| S15-D | 318mA校准数据实机采集 | in_progress | 2026-07-13-bq27220-config-readback-calibration |
| S15-E | CC Gain与CC Delta校准值写入验证 | pending | 2026-07-13-bq27220-config-readback-calibration |
| S15-F | SOC/FCC完整放电回归验证 | pending | 2026-07-13-bq27220-config-readback-calibration |
| S15-G | 充电终止参数选择性回读与修复 | completed | 2026-07-13-bq27220-config-readback-calibration |
| S15-H | 200mA/50mV满充识别实机验证 | pending | 2026-07-13-bq27220-config-readback-calibration |
| S16-A | UART1 AUX GP42/GP43初始化与四页快照格式化 | completed | 2026-07-14-bq27220-battery-uart-logging |
| S16-B | 100/75/50/25/15/10/7/3/0%精确档位单次触发（历史实现，S18-A已替代） | completed | 2026-07-14-bq27220-battery-uart-logging |
| S16-C | 串口抓取与完整放电实机验证 | pending | 2026-07-14-bq27220-battery-uart-logging |
| S17-A | BQ27220有效SOC生成7%低电灯光保护状态 | completed | 2026-07-14-low-battery-led-cutoff |
| S17-B | GP22/GP40统一渲染入口强制全黑并自动恢复 | completed | 2026-07-14-low-battery-led-cutoff |
| S17-C | 8%/7%/3%/0%及充电恢复实机验证 | pending | 2026-07-14-low-battery-led-cutoff |
| S18-A | BQ27220每2秒周期输出UART1四页快照 | completed | 2026-07-15-battery-uart-oled-sleep-icon |
| S18-B | GP30/31/32活动时间戳与OLED 60秒休眠/输入唤醒 | completed | 2026-07-15-battery-uart-oled-sleep-icon |
| S18-C | BUTTONS右上角四格电池图标和右下角百分比，并移除V/I/FCC诊断数值 | completed | 2026-07-15-battery-uart-oled-sleep-icon |
| S18-D | 串口周期、OLED休眠唤醒与主页面实机验证 | pending | 2026-07-15-battery-uart-oled-sleep-icon |
| S19-A | Battery Info层级0入口板级开关并默认隐藏 | completed | 2026-07-15-hide-battery-info-menu |
| S19-B | Battery Info四页绘制与导航保留检查 | completed | 2026-07-15-hide-battery-info-menu |
| S19-C | 层级0菜单顺序与调试入口恢复实机验证 | pending | 2026-07-15-hide-battery-info-menu |
| S20-A | RGB亮度档位配置、默认值与持久化 | completed | 2026-07-15-rgb-brightness-levels |
| S20-B | Brightness菜单、OLED标记与All OFF索引安全 | completed | 2026-07-15-rgb-brightness-levels |
| S20-C | Key/Base指定六个效果分支应用三档亮度 | completed | 2026-07-15-rgb-brightness-levels |
| S20-D | 配置、菜单和渲染路径静态验证 | completed | 2026-07-15-rgb-brightness-levels |
| S20-E | 三档亮度、重启恢复和效果隔离实机验证 | pending | 2026-07-15-rgb-brightness-levels |
| S22-A | GP24与VCC_5V/OVCC_3V3原理图供电范围核对 | completed | 2026-07-15-rgb-gp24-power-gating |
| S22-B | All OFF与灯光模式选择共享电源请求状态 | completed | 2026-07-15-rgb-gp24-power-gating |
| S22-C | 双灯链最终黑帧、GP24关断与上电延时 | completed | 2026-07-15-rgb-gp24-power-gating |
| S22-D | GP24单写入点、菜单与低电路径静态验证 | completed | 2026-07-15-rgb-gp24-power-gating |
| S22-E | 返工：All OFF后GP24仍为3.3V，复查运行路径与引脚接管 | in_progress | 2026-07-15-rgb-gp24-power-gating |

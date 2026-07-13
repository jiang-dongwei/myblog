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

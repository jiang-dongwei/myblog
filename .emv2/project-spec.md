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

### 蓝牙连接状态弹窗与 Base 灯效
- 物理硬件: 复用 RP2350B UART0 GP44/GP45、SSD1306 OLED、GP40 的 19 颗 Base LED 和 GP24 RGB 电源门控
- 功能: 接收 ESP32-C6 `0x46 0x53` 状态帧，自动显示 Pairing/Connecting/Connected/Disconnected 页面，并用临时蓝色 Chase、全蓝或全黑 Base 灯效反馈连接状态后无损恢复
- 讨论ID: `2026-07-20-bluetooth-status-popup-led`

### 菜单期间游戏输入锁定与 ESP32-C6 协同
- 物理硬件: 复用 GP2～GP20 游戏按键、GP19 菜单 BACK、GP30/GP31/GP32 菜单控制和 RP2350B UART0 到 ESP32-C6 的既有链路
- 功能: 菜单打开后锁定 USB 游戏输出和本地 Turbo/宏/热键副作用；退出后等待 GP2～GP20 全部释放 30ms；`FP` Byte4 bit7 将锁状态交给 ESP32-C6 执行 BLE HID 最终中和与排空
- 讨论ID: `2026-07-21-menu-game-input-lockout`

### 蓝牙状态双段 Chase
- 物理硬件: 无新增硬件，复用 GP40 的 19 颗 Base LED
- 功能: 保留原单段蓝色 Chase，并通过板级条件编译切换到两段近似对角、每段 `80%/25%/5%` 拖尾、`50ms/格` 的蓝色 Chase
- 讨论ID: `2026-07-22-bluetooth-dual-chase`

### Fightpad12Slim 量产默认启动 Logo
- 物理硬件: 复用 I2C0 GP0/GP1 的现有 128×64 单色 OLED
- 功能: 将用户提供的 1024 字节字模固化为 Fightpad12Slim 板级默认静态启动 Logo，量产新设备无需逐台通过 Web Config 上传
- 讨论ID: `2026-07-31-fightpad-default-splash-logo`

### Web Config Fightpad 品牌
- 物理硬件: 无硬件变化，只修改设备内置 Web Config 前端资源
- 功能: 导航栏使用与 OLED 启动图一致的方框 R Logo 和 `FIGHTPAD` 粗斜体字标，并同步首页与浏览器品牌元数据
- 讨论ID: `2026-07-31-webconfig-fightpad-branding`

### Key/Base 统一灯效与 Chase 加速
- 物理硬件: 无新增硬件，复用 GP22 的 12 颗 Key LED、GP40 的 19 颗 Base LED 和 GP24 RGB 电源门控
- 功能: 将 Key Effect 与 Base Effect 合并为 Light Effect，两条灯链共享模式、颜色和动画相位，并将普通 Chase 从 200ms/格加快到 160ms/格
- 讨论ID: `2026-08-07-unified-light-effect-chase-speed`

### GP30 短按持久化开关普通灯效
- 物理硬件: 复用 GP30 拨轮按键、GP22 Key 灯链、GP40 Base 灯链和 GP24 RGB 电源门控
- 功能: 菜单关闭时短按 GP30 持久化关闭或恢复普通 Light Effect 与 Key Flash；菜单 All OFF 保持原覆盖行为，长按菜单不变，蓝牙 GP40 临时灯效保持现有行为
- 讨论ID: `2026-08-07-gp30-short-press-light-toggle`

### GP33传输选择与GP34 ESP32-C6使能
- 物理硬件: GP33为USB/BT拨杆输入；GP34连接ESP32-C6 CHIP_PU/EN并高有效
- 功能: 按装配实机挡位，GP33低选择USB并拉低GP34，GP33高选择BT并拉高GP34；运行时切换需稳定30ms，上电首次采样立即生效
- 讨论ID: `2026-08-10-gp33-controls-esp32-enable`

### GP22 Chase 2秒周期与任意按键唤醒OLED
- 物理硬件: GP22的12颗按键灯、GP2～GP20游戏按键、GP30～GP32拨轮控制和I2C OLED
- 功能: GP22与GP40普通Chase均为2秒一圈；OLED保持60秒空闲休眠，并可由GP2～GP20任意按键、GP30～GP32或蓝牙状态事件唤醒
- 讨论ID: `2026-08-10-gp22-chase-oled-any-button-wake`

### PS3残留设备类型容错
- 物理硬件: 无变化，复用现有USB控制器模式与拨轮菜单
- 功能: Controller Type选择普通模式时清除Web Config残留的设备子类型；PS3启动时将不支持的Arcade/HOTAS/Mecha类型兜底为普通Gamepad；PS3对外输入报告严格匹配HID描述符的49字节长度
- 讨论ID: `2026-08-10-ps3-device-type-normalization`

### 启动画面保护与绑定设备蓝牙准入
- 物理硬件: 复用 RP2350B OLED、RP2350B↔ESP32-C6 UART0 和 ESP32-C6 GPIO13 配对按键
- 功能: 3秒 Splash 期间禁止蓝牙状态覆盖；权威 C6 工程为 `E:\WorkSpace\C_WorkSpacee\ESP-IDF5.2\.espressif\release-v5.2\esp32c6_ble_hid_gamepad_test`，非显式配对时仅向最近绑定设备定向广播，无绑定时等待 GPIO13 的30秒窗口，并拒绝非授权的新连接/重复配对
- 讨论ID: `2026-08-11-splash-bonded-peer-guard`

### USB独立的多类型BLE控制器Profile
- 物理硬件: 复用 RP2350B↔ESP32-C6 UART0、GP33/GP34传输使能、GPIO13配对按键和现有OLED
- 功能: 新增独立 Bluetooth Type 菜单，支持 Xbox/Generic/Keyboard/PS5-PC；RP2350持久化选择并通过版本化Mode/ACK协议同步到C6，Profile变化时仅重启C6并进入30秒重新配对
- 讨论ID: `2026-08-12-ble-controller-profiles`

### 当前传输控制器类型统一显示
- 物理硬件: 无新增硬件，复用 GP33 传输选择、RP2350↔ESP32-C6 UART0 和现有 OLED
- 功能: USB挡位按RP2350 InputMode和上游原始标签显示，BT挡位按C6 `FA` ACK确认的
  BLE Profile显示；BLE Xbox使用`XINPUT`、BLE PS4使用`PS4`、BLE Switch使用`SWITCH`，并同步输入历史按钮命名
- 讨论ID: `2026-08-13-active-transport-controller-label`

### BLE Profile强制持久化与同步门控
- 物理硬件: 无新增硬件，复用RP2350 Flash、拨轮菜单和RP2350到ESP32-C6 UART0
- 功能: Bluetooth Type选择使用强制保存但不重启RP2350；Flash成功后才允许发送Mode，失败时恢复旧RAM值并显示Save Failed，保持USB InputMode与BLE Profile独立
- 讨论ID: `2026-08-13-ble-profile-persistence`

### Switch BLE Profile 4
- 物理硬件: 无新增硬件，复用RP2350到ESP32-C6 UART0、Bluetooth Type菜单和现有OLED
- 功能: 在旧Profile编号不变的前提下追加Switch=4，菜单选择后复用现有强制保存、Mode/ACK、C6独立重启和多Profile Bond隔离流程
- 讨论ID: `2026-08-17-switch-ble-profile-4`

### BLE控制器选择后返回主页面
- 物理硬件: 无新增硬件，复用拨轮菜单、OLED和现有BLE Profile异步保存流程
- 功能: Bluetooth Type列表确认任一Profile后立即退出拨轮菜单并恢复BUTTONS页面，保存和C6同步提示继续异步执行
- 讨论ID: `2026-08-17-ble-profile-return-buttons`

### BLE控制器菜单精简
- 物理硬件: 无新增硬件，复用Bluetooth Type菜单和现有BLE Profile协议
- 功能: 量产菜单隐藏Keyboard入口并将PS4 BLE (PC)标签统一简化为PS BLE；Profile 2/3编号和旧配置兼容保持不变
- 讨论ID: `2026-08-17-ble-menu-trim`

### BLE控制器切换可见重启
- 物理硬件: 无新增硬件，复用RP2350 watchdog重启、OLED启动Logo和RGB黑屏反馈
- 功能: Bluetooth Type实际变化并保存后执行与USB Controller Type相同的500ms黑屏和RP2350重启；相同Profile不重启
- 讨论ID: `2026-08-17-ble-profile-visible-reboot`

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
| S23-A | UART `0x53` 状态帧解析与跨核事件快照 | completed | 2026-07-20-bluetooth-status-popup-led |
| S23-B | OLED 蓝牙状态临时覆盖页与休眠唤醒 | completed | 2026-07-20-bluetooth-status-popup-led |
| S23-C | GP40 蓝色 Chase/静态/关闭临时覆盖与原灯效恢复 | completed | 2026-07-20-bluetooth-status-popup-led |
| S23-D | 协议、显示、灯效和优先级非编译静态验证 | completed | 2026-07-20-bluetooth-status-popup-led |
| S23-E | Pairing/Connecting成功或失败、All OFF、休眠和低电实机验证 | pending | 2026-07-20-bluetooth-status-popup-led |
| S24-A | RP2350 菜单锁存、退出释放排空与 GP19 菜单功能保留 | completed | 2026-07-21-menu-game-input-lockout |
| S24-B | USB neutral 门控和 Turbo/宏/普通及重启热键副作用保护 | completed | 2026-07-21-menu-game-input-lockout |
| S24-C | `FP` Byte4 bit7 锁标志与 ESP32-C6 共享实施文档 | completed | 2026-07-21-menu-game-input-lockout |
| S24-D | RP2350 代码、协议帧和文档非编译静态验证 | completed | 2026-07-21-menu-game-input-lockout |
| S24-E | ESP32-C6 实现、双端构建烧录和 USB/BLE 实机回归 | pending | 2026-07-21-menu-game-input-lockout |
| S25-A | 蓝牙状态 Chase 条件编译与双段对角拖尾实现 | completed | 2026-07-22-bluetooth-dual-chase |
| S25-B | 新旧 Chase 分支、速度、亮度和状态隔离非编译静态验证 | completed | 2026-07-22-bluetooth-dual-chase |
| S25-C | Pairing/Connecting、All OFF 和低电优先级实机验证 | pending | 2026-07-22-bluetooth-dual-chase |
| S26-A | Fightpad12Slim 板级 128×64 默认启动 Logo 接入 | completed | 2026-07-31-fightpad-default-splash-logo |
| S26-B | 字节数、源字模一致性与配置路径非编译静态验证 | completed | 2026-07-31-fightpad-default-splash-logo |
| S26-C | 构建烧录、3 秒显示与恢复出厂配置实机验证 | pending | 2026-07-31-fightpad-default-splash-logo |
| S27-A | Fightpad SVG Logo 与导航栏 `R + FIGHTPAD` 字标 | completed | 2026-07-31-webconfig-fightpad-branding |
| S27-B | 多语言首页品牌与浏览器元数据同步 | completed | 2026-07-31-webconfig-fightpad-branding |
| S27-C | SVG、资源引用、响应式样式与补丁非编译静态检查 | completed | 2026-07-31-webconfig-fightpad-branding |
| S27-D | 返工：移除导航破图节点并将 FIGHTPAD 文字左移 | in_progress | 2026-07-31-webconfig-fightpad-branding |
| S29-A | 合并灯效菜单与旧配置双向映射 | completed | 2026-08-07-unified-light-effect-chase-speed |
| S29-B | GP22/GP40 共享动画状态与普通 Chase 160ms/格 | completed | 2026-08-07-unified-light-effect-chase-speed |
| S29-C | 菜单、映射、速度和优先级非编译静态验证 | completed | 2026-08-07-unified-light-effect-chase-speed |
| S29-D | 构建烧录与五种效果实机回归 | pending | 2026-08-07-unified-light-effect-chase-speed |
| S30-A | 当前 GP30 状态机发布菜单外短按灯光开关 | completed | 2026-08-07-gp30-short-press-light-toggle |
| S30-B | 普通灯效运行时门控并保留蓝牙 GP40 临时覆盖 | completed | 2026-08-07-gp30-short-press-light-toggle |
| S30-C | 短按、长按、蓝牙、All OFF 与低电优先级验证 | pending | 2026-08-07-gp30-short-press-light-toggle |
| S31-A | Controller Type 菜单与八项上游 InputMode 映射 | completed | 2026-08-07-controller-type-menu-chase-100ms |
| S31-B | 当前项标记、变更保存与 USB 重启重新枚举 | completed | 2026-08-07-controller-type-menu-chase-100ms |
| S31-C | 菜单导航、模式映射、保存路径非编译静态验证 | completed | 2026-08-07-controller-type-menu-chase-100ms |
| S31-D | 构建烧录与八种有线 USB 模式实机验证 | pending | 2026-08-07-controller-type-menu-chase-100ms |
| S32-A | 普通 Light Effect Chase 由160ms调整为100ms，蓝牙50ms不变 | completed | 2026-08-07-controller-type-menu-chase-100ms |
| S32-B | 普通双灯链同步与蓝牙状态 Chase 实机验证 | pending | 2026-08-07-controller-type-menu-chase-100ms |
| S33-A | 菜单非OFF灯效选择同步恢复GP30手动灯光ON并持久化 | completed | 2026-08-07-menu-effect-enables-gp30-light |
| S33-B | 非OFF状态写入、OFF隔离与保存顺序非编译静态验证 | completed | 2026-08-07-menu-effect-enables-gp30-light |
| S33-C | GP30关闭后菜单恢复灯光与重启持久化实机验证 | pending | 2026-08-07-menu-effect-enables-gp30-light |
| S34-A | 控制器模式变更后以RAM黑屏标志提供明显重启反馈 | completed | 2026-08-07-controller-mode-visible-reboot |
| S34-B | 黑屏优先级、非持久化与watchdog路径非编译静态验证 | completed | 2026-08-07-controller-mode-visible-reboot |
| S34-C | 500ms熄灯、USB重枚举与启动后灯效恢复实机验证 | pending | 2026-08-07-controller-mode-visible-reboot |
| S35-A | 普通GP40和GP22 Chase改为两个三灯拖尾段 | completed | 2026-08-07-normal-dual-segment-chase |
| S35-B | 半圈间隔、共享步进、速度隔离与Key Flash非编译静态验证 | completed | 2026-08-07-normal-dual-segment-chase |
| S35-C | 双段追逐、两链同步和按键闪光实机验证 | pending | 2026-08-07-normal-dual-segment-chase |
| S36-A | GP22一秒与GP40两秒独立整圈周期时间相位（历史实现，S38-A已替代） | completed | 2026-08-07-independent-chase-cycle-speed |
| S36-B | 周期公式、双段几何、共享颜色和蓝牙速度隔离静态验证 | completed | 2026-08-07-independent-chase-cycle-speed |
| S36-C | 历史1秒/2秒周期实机验证（由S38-D替代） | superseded | 2026-08-07-independent-chase-cycle-speed |
| S37-A | GP33/GP34板级配置与C6 EN跟随驱动 | completed | 2026-08-10-gp33-controls-esp32-enable |
| S37-B | 实机返工为USB低有效/BT高有效并保持Proxy路径一致 | completed | 2026-08-10-gp33-controls-esp32-enable |
| S37-C | 真值表、单一写入者、GP35与resetPin隔离静态验证 | completed | 2026-08-10-gp33-controls-esp32-enable |
| S37-D | GP33/GP34电平、C6广播与USB/BT实机验证 | pending | 2026-08-10-gp33-controls-esp32-enable |
| S37-E | GP33 30ms非阻塞消抖与双执行路径一致性 | completed | 2026-08-10-gp33-controls-esp32-enable |
| S38-A | GP22普通Chase由1秒调整为2秒一圈 | completed | 2026-08-10-gp22-chase-oled-any-button-wake |
| S38-B | GP2～GP20任意已消抖按键刷新OLED活动时间并唤醒 | completed | 2026-08-10-gp22-chase-oled-any-button-wake |
| S38-C | Chase周期、60秒休眠和跨核唤醒路径非编译静态验证 | completed | 2026-08-10-gp22-chase-oled-any-button-wake |
| S38-D | 构建烧录、双链周期与全部按键唤醒实机验证 | pending | 2026-08-10-gp22-chase-oled-any-button-wake |
| S39-A | Controller Type保存时归一为普通Gamepad并允许同模式纠错重启 | completed | 2026-08-10-ps3-device-type-normalization |
| S39-B | PS3驱动启动时将不支持的设备子类型兜底为普通Gamepad | completed | 2026-08-10-ps3-device-type-normalization |
| S39-C | PS3描述符、报告分支与菜单保存路径非编译静态验证 | completed | 2026-08-10-ps3-device-type-normalization |
| S39-D | 构建烧录并验证PS3按键输入及其他USB模式回归 | in_progress | 2026-08-10-ps3-device-type-normalization |
| S39-E | PS3中断IN与GET_REPORT长度统一为描述符声明的49字节 | completed | 2026-08-10-ps3-device-type-normalization |
| S40-A | Splash期间抑制蓝牙OLED覆盖 | completed | 2026-08-11-splash-bonded-peer-guard |
| S40-B | C6绑定设备定向广播与无绑定静默 | completed | 2026-08-11-splash-bonded-peer-guard |
| S40-C | C6陌生连接和Repeat Pairing准入保护 | completed | 2026-08-11-splash-bonded-peer-guard |
| S40-D | 双端构建烧录与启动/回连/新配对实机回归 | pending | 2026-08-11-splash-bonded-peer-guard |
| S41-A | BLE Profile配置字段、默认值与Bluetooth Type菜单 | completed | 2026-08-12-ble-controller-profiles |
| S41-B | RP2350 Mode发送、ACK解析、sequence与超时重试 | completed | 2026-08-12-ble-controller-profiles |
| S41-C | OLED Applying/Pair Again/Protocol Error状态接入 | completed | 2026-08-12-ble-controller-profiles |
| S41-D | 双工程协议、映射、状态机与验收交接文档 | completed | 2026-08-12-ble-controller-profiles |
| S41-E | RP侧非编译静态验证及双端构建烧录实机联调 | in_progress | 2026-08-12-ble-controller-profiles |
| S41-F | GPIO13配对回归定位与ESP32-C6精确修复任务书 | completed | 2026-08-12-ble-controller-profiles |
| S41-G | C6端Pairing优先级、按键边沿、断链广播与双端实机验证 | pending | 2026-08-12-ble-controller-profiles |
| S42-A | 发布已消抖BT挡位与C6 ACK确认Profile的跨核快照 | completed | 2026-08-13-active-transport-controller-label |
| S42-B | BUTTONS主页面和输入历史按当前传输显示类型并保留USB原始标签 | completed | 2026-08-13-active-transport-controller-label |
| S42-C | ACK前回退、USB/BT隔离、标签映射和差异非编译静态验证 | completed | 2026-08-13-active-transport-controller-label |
| S42-D | RP2350构建烧录与USB/BT Xbox/PS4/Switch主页面实机验证 | pending | 2026-08-13-active-transport-controller-label |
| S43-A | BLE Profile强制保存且不重启RP2350 | completed | 2026-08-13-ble-profile-persistence |
| S43-B | 保存完成前Mode同步门控及失败回滚与OLED提示 | completed | 2026-08-13-ble-profile-persistence |
| S43-C | has有效位、USB/BLE独立、APPLY_NOW和XOR向量静态验证 | completed | 2026-08-13-ble-profile-persistence |
| S43-D | PS/Xbox切换、普通重启、超时和USB/BLE独立实机验证 | pending | 2026-08-13-ble-profile-persistence |
| S44-A | Profile 4公共枚举、合法范围、标签和Bluetooth Type菜单接入 | completed | 2026-08-17-switch-ble-profile-4 |
| S44-B | Proxy、配置存储、ACK及主页面Switch/PS4显示路径审计补齐 | completed | 2026-08-17-switch-ble-profile-4 |
| S44-C | 主协议文档、固定UART向量和非编译静态验证 | completed | 2026-08-17-switch-ble-profile-4 |
| S44-D | 用户双固件构建烧录、Switch配对重连和多Profile Bond回归 | pending | 2026-08-17-switch-ble-profile-4 |
| S45-A | Bluetooth Type确认后复用菜单退出路径返回BUTTONS页面 | completed | 2026-08-17-ble-profile-return-buttons |
| S45-B | 用户构建烧录并验证保存、提示覆盖和菜单输入解锁 | pending | 2026-08-17-ble-profile-return-buttons |
| S46-A | Bluetooth Type隐藏Keyboard入口并将PS标签改为PS BLE | completed | 2026-08-17-ble-menu-trim |
| S46-B | Profile 2/3协议编号、旧配置和菜单计数非编译静态验证 | completed | 2026-08-17-ble-menu-trim |
| S46-C | 用户重新构建烧录并验证菜单与切换提示 | pending | 2026-08-17-ble-menu-trim |
| S47-A | BLE Profile变化复用USB模式黑屏、强制保存和RP2350重启路径 | completed | 2026-08-17-ble-profile-visible-reboot |
| S47-B | 保存顺序、Proxy同步窗口、相同Profile不重启和文档静态验证 | completed | 2026-08-17-ble-profile-visible-reboot |
| S47-C | 用户构建烧录并验证启动Logo、C6切换及Bond恢复 | pending | 2026-08-17-ble-profile-visible-reboot |

# 需求确认: BQ27220 电量 SOC/FCC 稳定性

## 1. BQ27220 配置与初始化

### 已确认电池参数

- Design Capacity: 650 mAh
- Design Energy: 2405 mWh
- Design Voltage: 3700 mV
- Taper Current: 20 到 30 mA，当前目标值使用 25 mA
- Battery Min Voltage: 2750 mV
- Battery Max Voltage: 4200 mV
- 电量计: BQ27220

### 当前固件行为

- `FIGHTPAD12SLIM_BQ27220_CONFIGURE_RAM = 1`
- 开机后进入 Full Access / Config Update
- 当前会写入 Charging Voltage、Taper Current、Full Charge Capacity、Design Capacity、Design Voltage、EDV0/1/2、Voltage 0 DOD、Voltage 100 DOD
- 当前也会把 Full Charge Capacity 写为 650

### 需求确认

- 正常开机不应无条件覆盖 BQ27220 学习得到的 FCC。
- 写 FCC=650 应只用于校准/恢复出厂电池配置模式，不能在低电 brownout 重启后自动执行。
- 正常运行应优先读取 SOC、Voltage、Current、FCC、OperationStatus 等状态。
- 固定设计参数可以保留写入/校验机制，但需要避免破坏电量计的学习结果。

## 2. BQ27220 SOC/FCC 跳变诊断

### 已确认异常

- OLED 显示 75% 时电压约 3781 mV。
- 电量到 52% 后直接跳变到 7%。
- FCC 从 650 跳变成 522。
- 5% 电量时电压约 3556 mV。
- 0% 电量时电压约 3512 mV。

### 需求确认

- 3512 mV 不应直接等同真实 0%，需要诊断 BQ27220 模型、EDV/DOD、学习状态或配置是否不匹配。
- OLED 诊断页需要保留 SOC、Voltage、Current、FCC、Security Status、配置写入/校验状态。
- 放电测试需要记录 SOC 跳变前后的连续数据。
- 需要能区分读数失败、配置失败、模型不稳定和真实低电。

## 3. 低电压与系统重启保护

### 已确认异常

- 低电量耗尽时灯乱闪。
- 程序反复重启。
- 重启后 FCC 又恢复为 650。

### 需求确认

- 低电量保护不改变 OLED 正常显示页面。
- 低电量保护时仍保持正常 BQ 配置流程。
- 低电量保护优先关注高耗电灯效/电源稳定性，但具体策略需避免影响 RP2350、ESP32 和 BQ27220 的既有工作路径。
- 低电阈值不能等到 MCU 反复 brownout 后才处理。

## 4. RP2350 与 ESP32 电量信息链路

### 用户确认

- RP2350 与 ESP32 电量信息链路使用同一套可信电量来源。
- 当前重点问题不是双源冲突，而是低电量反复重启后 FCC 又恢复为 650。

### 需求确认

- BQ27220 是唯一可信电量来源。
- ESP32 相关代码本阶段不改动。
- 本阶段只处理 RP2350/BQ27220 侧诊断、配置与稳定性问题。
- GP41 ADC 电压估算如已存在，仅作为既有路径保留，不在本阶段改动。

## 5. OLED 电量显示与调试信息

### 需求确认

- OLED 保持当前正常显示行为，不新增低电保护页面，不改变正常显示逻辑。
- 现有诊断显示可用于人工观察 SOC、Voltage、Current、FCC、状态码。
- 本阶段不新增 OLED 页面级交互。

## 6. 实机验证流程

### 需求确认

本阶段先不实现固件日志功能，不增加串口日志、Flash 日志或 ESP32 侧日志。放电验证先通过现有 OLED 诊断显示进行人工记录，至少记录以下字段：

- 时间
- SOC %
- Voltage mV
- Current mA
- FCC mAh
- 是否 VBUS
- 是否发生重启
- OLED 状态码
- LED 是否进入低电保护

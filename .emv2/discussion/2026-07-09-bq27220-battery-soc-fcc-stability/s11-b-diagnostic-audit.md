# S11-B 审计: BQ27220 诊断读取覆盖情况

## 审计结论

在“不新增日志、不改 OLED 正常显示、不改 ESP32”的约束下，当前 OLED 诊断能力已经覆盖 S11-C 人工复测所需的关键字段。

## 当前已读取字段

`FightpadBQ27220BatteryAddon` 当前已经读取并缓存：

- StateOfCharge: SOC %
- Voltage: 电池电压 mV
- Current: 电流 mA
- FullChargeCapacity: FCC mAh
- ReadStatus: 读取/配置状态
- Security Status: BQ27220 安全状态简码
- Data Memory Debug: 配置写入时的地址、旧值、目标值、校验值和 checksum

## 当前 OLED 诊断显示

`ButtonLayoutScreen::drawFightpadBatteryDiagnosticValues()` 当前显示：

- `SOC:xxx%`
- `V:xxxx`
- `I:+/-xxxx`
- `FCC:xxxxx`
- BQ 读取/配置错误码
- Security 状态简码

## 判断

S11-C 人工复测所需字段已经具备，不需要为 S11-B 新增固件代码。

## 不涉及的改动

本审计不修改固件代码，不改 ESP32，不改 OLED 正常显示，不新增日志。


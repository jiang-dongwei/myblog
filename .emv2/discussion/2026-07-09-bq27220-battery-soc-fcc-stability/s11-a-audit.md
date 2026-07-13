# S11-A 审计: BQ27220 配置写入行为

## 审计结论

当前固件在 `FIGHTPAD12SLIM_BQ27220_CONFIGURE_RAM = 1` 时，会在 `FightpadBQ27220BatteryAddon::process()` 的首次轮询中调用 `configureBatteryGauge()`。

由于 `batteryConfigAttempted` 和 `batteryConfigApplied` 是运行时成员变量，RP2350 重启后会重新初始化。因此每次 RP2350 重启后，都会再次尝试 BQ27220 配置流程。

## 当前会写入的 Data Memory 项

| 地址 | 项目 | 写入值来源 |
|------|------|------------|
| `0x91FD` | Charging Voltage | `FIGHTPAD12SLIM_BQ27220_BATTERY_MAX_VOLTAGE_MV` = 4200 |
| `0x9201` | Taper Current | `FIGHTPAD12SLIM_BQ27220_TAPER_CURRENT_MA` = 25 |
| `0x929D` | Full Charge Capacity | `FIGHTPAD12SLIM_BQ27220_DESIGN_CAPACITY_MAH` = 650 |
| `0x929F` | Design Capacity | `FIGHTPAD12SLIM_BQ27220_DESIGN_CAPACITY_MAH` = 650 |
| `0x92A3` | Design Voltage | `FIGHTPAD12SLIM_BQ27220_DESIGN_VOLTAGE_MV` = 3700 |
| `0x92B4` | Fixed EDV0 | `FIGHTPAD12SLIM_BQ27220_EDV0_MV` = 2750 |
| `0x92B7` | Fixed EDV1 | `FIGHTPAD12SLIM_BQ27220_EDV1_MV` = 3000 |
| `0x92BA` | Fixed EDV2 | `FIGHTPAD12SLIM_BQ27220_EDV2_MV` = 3300 |
| `0x92BD` | Voltage 0 DOD | `FIGHTPAD12SLIM_BQ27220_BATTERY_MAX_VOLTAGE_MV` = 4200 |
| `0x92D1` | Voltage 100 DOD | `FIGHTPAD12SLIM_BQ27220_BATTERY_MIN_VOLTAGE_MV` = 2750 |

## FCC 回到 650 的解释

`writeDataMemoryWord()` 会先读取旧值；如果旧值和目标值不同，就写入目标值并校验。因此：

- 如果 BQ27220 的 FCC 当前为 522；
- RP2350 因低电 brownout 重启；
- 固件重新执行 `configureBatteryGauge()`；
- `BQ27220_DATA_FULL_CHARGE_CAPACITY` 会被写回 650。

这与用户观察到的“低电重启后 FCC 又恢复 650”吻合。

## 不涉及的改动

本审计不修改固件代码，不改 ESP32，不改 OLED 正常显示，不新增日志。


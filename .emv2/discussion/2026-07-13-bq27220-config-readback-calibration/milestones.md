# 子流程拆分: BQ27220 启动回读与电流校准

## S15-A: 启动配置完整回读

- 回读 ITPOR等效状态、充电终止参数、SOC Flag Config A、Battery ID、Battery Low、EDV0/1/2、CC Offset、Board Offset、CC Gain 和 CC Delta。
- 保存修复前、目标和修复后快照。
- 静态检查数据地址、长度、字节序和 F4 解码。

## S15-B: 配置差异选择性修复

- 选择性修复 Charging Voltage、Taper Current、Taper Voltage、SOC Flag、Battery Low 和 EDV0/1/2。
- 写入后逐项二次回读。
- Battery ID 只读；普通重启不写 FCC。
- ITPOR/默认恢复时才允许恢复必要配置和 FCC 初始基线。

## S15-C: Battery Info 菜单与分页显示

- 层级 0 新增 `Battery Info`。
- 增加运行数据、配置回读、校准信息和充电终止状态四页。
- GP31/GP32 和 GP30 短按翻页，GP19 返回。
- 保持用户原有 `BUTTONS` 初始页和诊断编译开关状态不变。

## S15-D: 318 mA 校准数据采集

- 记录无负载 BQ 电流、CC Offset 和 Board Offset。
- 在 318 mA 标称负载下记录外部实际电流、BQ Current、CC Gain 和 CC Delta。
- 连续记录至少 5 组稳定数据。

## S15-E: 写入校准后的 CC Gain/CC Delta

- 根据 S15-D 实测数据计算并编码 F4 校准值。
- 增加 Fightpad12Slim 板级校准参数。
- ITPOR/配置丢失时恢复，普通启动只检查。
- 写入后逐字节回读验证，目标电流误差不超过约 3%。

## S15-F: SOC/FCC 完整放电验证

- 确认实际 EDV2 为 3300 mV。
- 复测约 3556 mV 的 7% 跳变、FCC 降低和约 3500 mV 显示 0% 的现象。
- 记录完整 SOC、电压、电流、RM 和 FCC 变化。

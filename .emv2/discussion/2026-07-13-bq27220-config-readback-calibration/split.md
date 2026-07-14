# 需求拆分: BQ27220 启动回读与电流校准

## 1. 启动配置回读

- 类型：通信/测量
- 简述：启动时通过软件 I2C 实际回读 ITPOR、Battery ID、Battery Low、EDV0/1/2、CC Offset、Board Offset、CC Gain 和 CC Delta。

## 2. 配置差异修复

- 类型：状态机/控制
- 简述：保留修复前快照，选择性修复 Battery Low 和 EDV0/1/2，并在写入后再次回读验证；普通重启不得覆盖 Learned FCC。

## 3. 电池诊断菜单

- 类型：显示/人机交互
- 简述：层级 0 新增 `Battery Info`，进入后复用 OLED 电池诊断信息并支持四页手动切换。

## 4. CC Gain/CC Delta 校准

- 类型：测量/存储
- 简述：先显示实际校准参数和电流读数，再使用 10 mOhm 采样电阻与 318 mA 外部参考电流完成实机校准并固化结果。

## 5. SOC/FCC 回归验证

- 类型：验证
- 简述：校准后复测 EDV2、SOC 7% 跳变、FCC 学习和 0% 电压行为。

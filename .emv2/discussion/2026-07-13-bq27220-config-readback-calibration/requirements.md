# 需求确认: BQ27220 启动回读与电流校准

## 启动检查

- 实际回读 ITPOR等效状态、Charging Voltage、Taper Current、Taper Voltage、SOC Flag Config A、Battery ID、Battery Low、EDV0、EDV1、EDV2、CC Offset、Board Offset、CC Gain 和 CC Delta。
- 在任何自动写入前保存原始回读值。
- 充电终止参数、SOC Flag、Battery Low、EDV0/1/2 不一致时允许选择性修复并二次回读；局部修复不得重置FCC或CC校准。
- Battery ID 本阶段只回读，不自动修改。
- 状态使用 `OK`、`FIX`、`BAD` 和 `UNCAL`。
- 普通 RP2350 重启不得重写 Learned FCC；只有确认 BQ27220 RAM 已恢复默认时才恢复 FCC 初始基线。

## OLED 菜单

- 层级 0 新增 `Battery Info`。
- 第 1 页显示 SOC、电压、电流、RM 和 FCC。
- 第 2 页显示 Battery ID、Battery Low 和 EDV0/1/2 的实际值与结果。
- 第 3 页显示 CC Offset、Board Offset、CC Gain、CC Delta 和校准状态。
- 第 4 页显示 Charging Voltage、Taper Current、Taper Voltage、SOC Flag Config A、瞬时电流、AverageCurrent 和 FC/TCA。
- GP31/GP32 切换页面，GP30 短按切换下一页，GP19 返回层级 0，GP30 长按退出菜单。
- 页面不自动轮换。
- 保持用户原有 `BUTTONS` 初始页及其电池诊断显示，不修改该页面；新增回读信息只放在菜单的 `Battery Info` 页面。

## 电流校准

- 采样电阻为 10 mOhm。
- 标称校准电流为 318 mA，最终计算使用外部电流表实测值。
- 第一轮固件只回读并显示 CC Gain/CC Delta，不盲目写入理论值。
- 实测后把校准值作为 Fightpad12Slim 板级配置固化；普通启动只检查，ITPOR/配置丢失时才恢复。
- 按项目约定不在本仓库运行编译，由用户构建和实机验证。

# Custom Theme Lighting Menu - 硬件对齐

## 现有硬件

### GP22按键灯链

- 数据引脚：GP22
- 灯珠数量：12
- 格式：GRB
- 驱动：FightpadAmbientLEDAddon
- PIO资源：PIO2，状态机1
- 所有权：`FIGHTPAD12SLIM_AMBIENT_OWNS_GP22=1`

物理灯珠与逻辑按键保持现有映射：

| 灯珠索引 | 逻辑按键 |
|---:|---|
| 0 | Left |
| 1 | Down |
| 2 | Right |
| 3 | B3 |
| 4 | B4 |
| 5 | R1 |
| 6 | L1 |
| 7 | L2 |
| 8 | R2 |
| 9 | B2 |
| 10 | B1 |
| 11 | Up |

### GP40环境灯链

- 数据引脚：GP40
- 灯珠数量：19
- 格式：GRB
- 驱动：FightpadAmbientLEDAddon
- PIO资源：PIO2，状态机0

### 共用控制

- GP30继续作为现有灯光总开关。
- 继续复用既有亮度、供电门控、低电量和蓝牙临时状态覆盖逻辑。

## 新增硬件需求

- 无新增GPIO。
- 无新增PIO状态机。
- 无新增中断、DMA或通信接口。
- 不改变LED电气格式和灯珠数量。

## 冲突约束

- 上游NeoPico不得与FightpadAmbientLEDAddon同时写GP22。
- Web Config自定义主题只提供Flash中的颜色数据，不取得灯链驱动权。
- 不根据Web Config的GPIO按键映射改变本板固定的12灯物理顺序。


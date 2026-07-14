# BQ27220 电量计使用、配置与测试指南

本文面向 Fightpad12Slim 项目的开发和测试人员，说明 BQ27220 的基本原理、本产品电池参数、当前固件参数、配置写入方法、校准方法和放电测试方法。

本文以当前项目源码和 TI 官方资料为依据。BQ27220 的 Data Memory 参数和位定义最终应以对应芯片版本的 TI Technical Reference Manual 为准。

## 1. 本项目的电池系统

### 1.1 电池参数

| 参数 | 当前值 | 含义 |
| --- | ---: | --- |
| 电池类型 | 单节锂电池，1S | BQ27220 用于单节电池 |
| Design Capacity | 650 mAh | 新电池标称容量 |
| Design Energy | 2405 mWh | 650 mAh x 3.7 V |
| Design Voltage | 3700 mV | 标称电压 |
| 最高充电电压 | 4200 mV | 满充目标电压 |
| 电芯规格最低电压 | 2750 mV | 电芯绝对下限，不等于产品建议工作下限 |
| Taper Current | 200 mA | BQ 满充识别阈值，匹配当前 TP4056 的实际截止行为 |
| Taper Voltage | 50 mV | 满充识别电压裕量，要求电压高于 4150 mV |
| Battery Low | 7.00% | EDV2 对应的 SOC |

### 1.2 硬件连接

BQ27220 直接由电池供电，不跟随 RP2350 和 ESP32-C6 的系统电源复位。其最低工作电压约为 2.4 V，低于电芯 2.75 V 规格下限。

当前引脚分配：

| RP2350 GPIO | BQ27220 信号 | 说明 |
| --- | --- | --- |
| GP25 | SCL | 软件 I2C 时钟 |
| GP26 | SDA | 软件 I2C 数据 |
| GP27 | GPOUT | 状态/中断输入，目前配置为输入 |

这意味着 RP2350 因低电压反复重启时，BQ27220 仍可能保持之前的 Learned FCC、DOD 和其他运行状态。

## 2. BQ27220 的基本概念

### 2.1 Design Capacity

Design Capacity，简称 DC，是电池设计容量。本产品为 650 mAh。

它描述新电池的标称容量，也参与 State of Health 等计算。它不是每次放电自动变化的学习值。

### 2.2 Remaining Capacity

Remaining Capacity，简称 RM，是 BQ27220 当前估计仍可使用的容量，单位为 mAh。

充电时 RM 增加，放电时 RM 减少。RM 还会在 EDV2、EDV1 和 EDV0 被电压阈值校正。

### 2.3 Full Charge Capacity

Full Charge Capacity，简称 FCC，是芯片学习得到的当前可用满充容量，单位为 mAh。

FCC 不是固定设计参数。BQ27220 在完成合格放电后，会根据实际累计放电量更新 FCC，并保存为 Learned Full Charge Capacity。

正常情况下，FCC 应逐渐收敛到电池在产品工作条件下的可用容量。如果 FCC 连续大幅下降，而 SOC=0 后产品仍能运行很久，说明学习条件、EDV 参数或电流校准可能有问题。

### 2.4 SOC

SOC 是芯片估算的剩余电量百分比。可以近似理解为：

```text
SOC ~= RM / FCC x 100%
```

实际算法还包含 EDV 校正、平滑、充电终止和初始 OCV 估算，因此不能只根据电池电压直接换算 SOC。

例如 FCC 错误地从 650 mAh 学习为 394 mAh 后：

```text
7% x 394 mAh = 27.6 mAh
```

芯片跳到 7% 后只需再累计约 28 mAh，就可能显示 0%，即使物理电池仍有较多容量。

### 2.5 Qmax

Qmax 表示电芯的最大化学容量，主要参与上电后的 OCV 初始容量估算。

对于本产品的一节 650 mAh 电池，初始配置通常应检查：

```text
Qmax Cell 1 = 650 mAh
Qmax Pack   = 650 mAh
```

Qmax、Design Capacity 和 FCC 含义不同，不能互相替代。

### 2.6 DCR 与容量学习

DCR 是放电计数寄存器。BQ27220 在满足条件的放电周期中累计放电量，并在达到 EDV2 后用于计算新的 FCC。

简化关系为：

```text
New FCC = discharge counted to EDV2 + Battery Low reserve + initial DCR
```

如果 EDV2 被过早触发，或库仑计把实际电流计算得过大，学习得到的 FCC 就会偏小。

## 3. EDV 电压等级

BQ27220 使用三个 End-of-Discharge Voltage 阈值校正剩余容量。

| 阈值 | SOC 目标 | 当前项目值 |
| --- | ---: | ---: |
| EDV2 | Battery Low % | 3300 mV -> 7% |
| EDV1 | 3% | 3000 mV |
| EDV0 | 0% | 2750 mV |

### 3.1 EDV_CMP

`EDV_CMP` 决定 EDV 阈值的计算方式：

| EDV_CMP | 工作方式 |
| ---: | --- |
| 0 | 使用 Fixed EDV0/1/2 |
| 1 | 根据 CEDV Profile、电流和温度动态计算 EDV0/1/2 |

在没有使用 TI GPC 工具生成本电池专用 CEDV 参数之前，本项目使用：

```text
EDV_CMP = 0
```

这样可以先在常温下验证固定阈值，避免使用不匹配的默认动态 Profile。

### 3.2 Battery Low 与 EDV2

本项目设置：

```text
Battery Low = 700 x 0.01% = 7.00%
Fixed EDV2  = 3300 mV
```

当芯片在有效放电中检测到 EDV2 时，可能把 RM 校正到 FCC 的 7%，因此 SOC 会直接跳到 7%。

这个跳变本身属于 BQ27220 的设计行为，但如果它发生在不合理的高电压，说明实际 EDV 配置、动态 Profile 或电流测量存在问题。

### 3.3 电芯最低电压与产品截止电压

必须区分两个概念：

```text
电芯规格最低电压：电芯允许的绝对下限
产品可用截止电压：整机仍能稳定运行的最低电池电压
```

电芯可以允许到 2.75 V，但 RP2350、ESP32、升压电路和灯效负载可能在更高电压就开始不稳定。

因此最终 EDV0 应根据产品在规定负载下的实际可用终点确定，而不是机械地照搬电芯绝对最低电压。产品还应设置独立于电量计的低电压降功耗或关机保护，避免 brownout 反复重启。

## 4. 充满识别

BQ27220 的主要充电终止检测需要同时满足：

1. 连续两个 Current Taper Window 内，充电电流低于 Taper Current。
2. 同一时间内累计充入容量满足最小变化要求。
3. 电池电压高于 `Charging Voltage - Taper Voltage`。

当前参数：

```text
Charging Voltage = 4200 mV
Taper Current     = 200 mA
Taper Voltage     = 50 mV
电压条件          = Voltage > 4150 mV
```

这里的 Taper Current 是 BQ27220 的满充识别阈值，不会改变 TP4056 的实际充电电流。当前硬件在约 120-140 mA 截止，若 BQ 仍使用 25 mA，则电流会从高于 25 mA 直接变为 0，无法同时满足容量继续增加的终止窗口。200 mA 用于首轮实机验证；通过 FC/TCA 验证后，可再尝试降低到 180 mA。

正确识别主充电终止后，应检查：

| 标志 | 含义 |
| --- | --- |
| FC | Full Charge，已识别充满 |
| TCA | Terminate Charge Alarm，充电终止提示 |

只有 OLED 显示 100% 并不能完全证明充满识别正确，测试时还应读取 FC 和 TCA。

## 5. 常用命令寄存器

以下为运行时常用的标准命令：

| 命令 | 地址 | 数据 | 当前用途 |
| --- | ---: | --- | --- |
| Voltage | 0x08 | mV，U16 | OLED 电池电压 |
| BatteryStatus | 0x0A | 状态位，U16 | FC、TCA、放电等状态 |
| Current | 0x0C | mA，I16 | OLED 电流 |
| RemainingCapacity | 0x10 | mAh，U16 | Battery Info 第 1 页显示 RM |
| FullChargeCapacity | 0x12 | mAh，U16 | OLED FCC |
| AverageCurrent | 0x14 | mA，I16 | BQ内部滤波后的平均电流，充电终止判断依据 |
| StateOfCharge | 0x2C | %，U16 | OLED SOC |
| OperationStatus | 0x3A | 状态位，U16 | 安全状态和配置模式 |
| DesignCapacity | 0x3C | mAh，U16 | 读取设计容量 |

BQ27220 标准字数据通常按低字节、再高字节读取。`Current()` 是每秒更新的瞬时值，`AverageCurrent()` 是芯片内部滤波后的平均值；Taper终止条件必须观察后者。

## 6. 当前使用的 Data Memory 参数

| 地址 | 参数 | 当前目标值 | 备注 |
| ---: | --- | ---: | --- |
| 0x9180 | CC Offset | 校准值 | 无电流校准 |
| 0x9184 | CC Gain | 校准值，F4 | 电流比例 |
| 0x9188 | CC Delta | 校准值，F4 | mAh 累计比例 |
| 0x91FD | Charging Voltage | 4200 mV | 充电终止条件 |
| 0x9201 | Taper Current | 200 mA | 充电终止条件 |
| 0x9251 | Battery Low % | 700 | 单位 0.01% |
| 0x926B | Near Full | 需要核对 | 默认值相对 650mAh 电池可能偏大 |
| 0x927F | SOC Flag Config A | 保留原值并置位终止标志使能 | 控制 FC/TCA |
| 0x929A | Battery ID | 需要核对 | 选择 CEDV Profile |
| 0x929B | CEDV Gauging Config | 清除 EDV_CMP | 保留其他位 |
| 0x929D | Full Charge Capacity | 仅在 BQ27220 RAM 恢复默认后写入 650 mAh | 初始化学习基线 |
| 0x929F | Design Capacity | 650 mAh | 设计参数 |
| 0x92A3 | Design Voltage | 3700 mV | 设计参数 |
| 0x92A5 | Taper Voltage | 50 mV | 充电终止条件 |
| 0x92B4 | Fixed EDV0 | 2750 mV | 当前调试值 |
| 0x92B7 | Fixed EDV1 | 3000 mV | 当前调试值 |
| 0x92BA | Fixed EDV2 | 3300 mV | 对应 7% |
| 0x92BD | Voltage 0% DOD | 4200 mV | 当前简化 OCV 端点 |
| 0x92D1 | Voltage 100% DOD | 2750 mV | 当前简化 OCV 端点 |

说明：仅设置 0% 和 100% DOD 两个端点不能替代完整电池 Profile。需要更高精度时，应通过 TI GPC/CEDV 工具根据实际电池放电数据生成参数。

## 7. 固件中的参数位置

板级目标参数位于：

```text
configs/Fightpad12Slim/BoardConfig.h
```

主要宏：

```cpp
FIGHTPAD12SLIM_BQ27220_DESIGN_CAPACITY_MAH
FIGHTPAD12SLIM_BQ27220_DESIGN_ENERGY_MWH
FIGHTPAD12SLIM_BQ27220_DESIGN_VOLTAGE_MV
FIGHTPAD12SLIM_BQ27220_TAPER_CURRENT_MA
FIGHTPAD12SLIM_BQ27220_TAPER_VOLTAGE_MV
FIGHTPAD12SLIM_BQ27220_BATTERY_LOW_PERCENT_X100
FIGHTPAD12SLIM_BQ27220_EDV_CMP
FIGHTPAD12SLIM_BQ27220_BATTERY_MIN_VOLTAGE_MV
FIGHTPAD12SLIM_BQ27220_BATTERY_MAX_VOLTAGE_MV
FIGHTPAD12SLIM_BQ27220_EDV0_MV
FIGHTPAD12SLIM_BQ27220_EDV1_MV
FIGHTPAD12SLIM_BQ27220_EDV2_MV
```

驱动实现位于：

```text
headers/addons/fightpad_bq27220_battery.h
src/addons/fightpad_bq27220_battery.cpp
```

运行在 RP2350 Core1，由 `GP2040Aux` 加载。当前轮询周期为 2 秒。

## 8. Data Memory 写入流程

BQ27220 的配置参数不是直接向参数地址发送普通 I2C 写入。当前固件使用 Manufacturer Access/Data Memory 窗口进行读写。

### 8.1 总体流程

```text
进入 Full Access
    -> ENTER_CFG_UPDATE (0x0090)
    -> 等待 CFGUPDATE 置位
    -> 逐项读取旧值
    -> 仅在值不同时写入新值
    -> 写 checksum 和 length
    -> 回读验证
    -> EXIT_CFG_UPDATE_REINIT (0x0091)
    -> 等待 CFGUPDATE 清零
```

### 8.2 单个 16 位参数写入

以地址 `AABB`、目标值 `CCDD` 为例：

```text
向 0x3E 写入地址低字节、地址高字节：BB AA
从 0x40 读取参数数据
向 0x3E 写入：BB AA CC DD
checksum = 0xFF - (BB + AA + CC + DD)
向 0x60 写入：checksum, 0x06
重新选择地址并回读验证
```

Data Memory 中的 16 位参数数据按高字节、低字节组织；地址选择字段按低字节、高字节发送。实现时必须区分这两种字节顺序。

### 8.3 读改写位字段

对于 CEDV Gauging Config 和 SOC Flag Config 等位字段，不能把整个字直接写成固定常量。正确做法是：

```text
读取当前值
    -> 只清除或设置目标 bit
    -> 保留其他 bit
    -> 写回并验证
```

当前项目管理 CEDV Config 的 `SC`、`EDV_CMP` 和 `CSYNC` 位，并保留其他配置：

```text
SC = 1       独立充电器
EDV_CMP = 0  使用固定 EDV
CSYNC = 1    有效充电终止后令 RM = FCC
```

### 8.4 FCC 的特殊处理

FCC 是 BQ27220 的学习值，不能在每次 RP2350 重启时覆盖。当前项目采用以下策略：

```text
RP2350 启动
    -> 读取 Design Capacity、充电终止参数和 CEDV Config
    -> 配置仍有效：不进入 CONFIG UPDATE，不写 FCC，不重新初始化
    -> BQ RAM 已恢复默认：写入配置和 FCC=650 mAh，再执行 REINIT
    -> 仅充电参数、SOC Flag 或 SC/EDV_CMP/CSYNC 不匹配：修正配置并用 0x0092 退出，不执行 REINIT
```

这样 RP2350 关机、重启不会破坏 BQ27220 在独立供电期间累计的 RM、SOC 和 Learned FCC。只有 BQ27220 自身掉电、RAM 参数恢复默认时，才重新建立 650 mAh 的初始学习基线。

### 8.5 启动回读与 Battery Info 页面

启动配置检查会先保存 Charging Voltage、Taper Current、Taper Voltage、SOC Flag Config A、Battery ID、Battery Low、EDV0/1/2、CC Offset、Board Offset、CC Gain 和 CC Delta 的真实回读值。充电参数、SOC Flag、Battery Low 或 EDV 不一致时，只修复不一致的项目，并再次回读确认；Battery ID 只读。

BQ27220 TRM 的状态机说明提到 `Flags()[ITPOR]`，但同一版本公开的 `BatteryStatus()` 位表没有给出该位。因此固件不猜测未定义位，而是用 Design Capacity 与默认配置证据生成 `RAM:INIT` / `RAM:KEEP` 判断。`RAM:INIT` 只表示本次启动检测到 RAM 配置需要重建。

层级 0 的 `Battery Info` 分为四页：

1. SOC、Voltage、Current、RM、FCC 和整体读回状态。
2. Battery ID、RAM 初始化判断、Battery Low 和 EDV0/1/2 的检查结果。
3. CC Offset、Board Offset、CC Gain、CC Delta 的解码值和原始 F4 字节。
4. Charging Voltage、Taper Current、Taper Voltage、SOC Flag Config A、瞬时 Current、AverageCurrent 和 FC/TCA。

GP31/GP32 可前后翻页，GP30 短按进入下一页，GP19 返回层级 0。`OK` 表示原值正确，`FIX` 表示本次已修复并回读一致，`BAD` 表示读取、解码或二次校验失败，`UNCAL` 表示尚未固化实测电流校准值。

Fightpad12Slim 原有的 `BUTTONS` 初始页及其电池诊断布局保持不变；`Battery Info` 是额外增加的菜单页面，不替换初始页。

## 9. 校准方法

校准应在电池 Profile 和学习测试之前完成。

### 9.1 电流零点校准

1. 断开充电器和负载，确保采样电阻没有电流。
2. 进入 Full Access。
3. 执行 CC Offset 和 Board Offset 校准。
4. 等待读数稳定并保存结果。

### 9.2 电流比例校准

1. 本板采样电阻为 10 mOhm，使用约 318 mA 的安全稳定放电负载。
2. 用可信万用表串联测量实际电流。
3. 连续记录至少 5 组稳定的实际电流和 BQ27220 `Current()`；最终计算使用电流表实测值，不使用负载标称值。
4. 同时记录第 3 页的 CC Gain、CC Delta 原始 F4 字节，以及 CC Offset 和 Board Offset。
5. 根据实测比例计算新 CC Gain/CC Delta，写入后逐字节回读验证。
6. 检查放电方向是否为负值，充电方向是否为正值，并确认 318 mA 附近误差不超过约 3%。

TI 校准指南使用较大电流作为示例，但本产品只有 650 mAh 电池。实际校准电流必须符合电芯、保护板、采样电阻和 PCB 的安全额定值，不能盲目照搬示例电流。

### 9.3 电压校准

1. 用万用表直接测量电池端电压。
2. 同时读取 BQ27220 `Voltage()`。
3. 在无大电流变化时执行电压校准。
4. 校准后在满电、中间电压和低电压至少检查三个点。

### 9.4 校准验收建议

| 项目 | 建议目标 |
| --- | --- |
| 电压误差 | 尽量控制在数十 mV 内 |
| 稳态电流误差 | 建议小于 3%-5% |
| 无负载电流 | 接近 0 mA，不应持续明显漂移 |
| 累计容量 | 与外部电量计/电子负载结果接近 |

## 10. 一次性初始化与重新学习

当 FCC 已错误学习为 522 或 394 mAh 时，普通 RP2350 重启不会再自动覆盖 FCC。需要重新建立学习基线时，应按以下顺序进行一次性恢复：

1. 完成电压、电流、Offset 校准。
2. 确认实际 `EDV_CMP=0`，而不仅是源码宏为 0。
3. 确认 Fixed EDV 和 Battery Low 实际写入成功。
4. 检查当前选择的 Battery Profile。
5. 一次性设置 Learned FCC=650 mAh。
6. 检查 Qmax Cell 1、Qmax Pack 和 DOD at EDV2。
7. 将 DOD at EDV2 按 Battery Low 重新初始化。
8. 完成一次正确的满充和受控放电学习。

DOD at EDV2 的初始化关系：

```text
DOD at EDV2 = (1 - Battery Low %) x 16384
```

Battery Low=7% 时，理论值约为：

```text
(1 - 0.07) x 16384 = 15237
```

实际写入应以 bqStudio、芯片版本和 TI 文档显示的字段格式为准。

## 11. 标准充放电测试流程

### 11.1 测试前准备

1. 固件配置写入无错误。
2. 电流和电压已经校准。
3. 测试环境保持常温并记录温度。
4. 使用相同灯效、屏幕亮度、ESP32 模式和负载条件。
5. 使用万用表或电子负载作为独立参考。
6. 测试期间不要插拔充电器，避免学习周期失效。

### 11.2 充满阶段

1. 充电到约 4.2 V。
2. 确认电压高于 4150 mV，并观察充电电流下降到 200 mA 以下。
3. 观察 AverageCurrent 低于 200 mA，并保持足够时间满足两个 40 秒 Taper Window。
4. 在 TP4056 截止前后确认 FC=1、TCA=1，并确认 RM=FCC、SOC=100%。
5. 记录 SOC、RM、FCC、电压和电流。

### 11.3 放电阶段

建议每 1-5 分钟记录：

| 时间 | SOC | Voltage | Current | RM | FCC | VDQ | EDV2 | EDV1 | EDV0 | 备注 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 00:00 | 100% |  |  |  |  |  |  |  |  | 开始放电 |
|  | 75% |  |  |  |  |  |  |  |  |  |
|  | 50% |  |  |  |  |  |  |  |  |  |
|  | 25% |  |  |  |  |  |  |  |  |  |
|  | 15% |  |  |  |  |  |  |  |  |  |
|  | 10% |  |  |  |  |  |  |  |  |  |
|  | 7% |  |  |  |  |  |  |  |  | EDV2 检查 |
|  | 3% |  |  |  |  |  |  |  |  | EDV1 检查 |
|  | 0% |  |  |  |  |  |  |  |  | EDV0/RM 检查 |

对于 650 mAh 电池，TI 规定合格放电在达到 EDV2 时，电流需要至少约为：

```text
3C/32 = 3 x 650 / 32 = 60.9 mA
```

如果负载低于条件、期间出现有效充电、温度过低或发生过载，FCC 学习周期可能无效。

### 11.4 0% 后的验证

SOC 变为 0% 时不要立刻认为电池已物理耗尽，应继续记录：

1. 0% 瞬间的负载电压。
2. 0% 后还能稳定运行多久。
3. 0% 后实际还能释放多少 mAh。
4. 去掉负载并静置 20-30 分钟后的开路电压。
5. 系统开始 brownout 或重启时的负载电压和电流。

如果 0% 后仍能释放大量容量，则 FCC、RM、EDV 或库仑计校准存在问题。

## 12. 本项目已观察到的异常

已记录的现象：

```text
FCC: 650 -> 522 -> 394 mAh
SOC: 约 50%/40% -> 7%
SOC=0% 时电压仍约 3.52 V
0% 后产品仍可运行一段时间
```

判断：

1. 40%-50% 跳到 7% 与 EDV2/Battery Low 校正行为一致。
2. 在较高电压过早触发说明实际 EDV 配置或动态 Profile 需要检查。
3. FCC 连续下降说明错误学习正在持续发生。
4. 0% 后仍有明显续航说明 Learned FCC/RM 已低估实际可用容量。
5. 需要优先核对芯片实际 RAM 配置和电流校准，而不是继续重复学习。

## 13. 低电保护与电量计的边界

BQ27220 负责估算电量，但不能代替系统电源保护。

产品应根据实测建立独立策略：

```text
正常电压
    -> 全功能运行
低电预警阈值
    -> 降低 GP22/GP40 灯效亮度或关闭高功耗效果
系统最低稳定电压
    -> 保存必要状态并受控关机/停止高负载
电池保护板截止
    -> 最后硬件保护，不应作为正常关机方式
```

低电保护阈值应使用带负载实测数据确定，并留出电压回差，避免在阈值附近反复开关和重启。

## 14. 故障排查顺序

遇到 SOC 跳变、FCC 异常或 0% 电压过高时，按以下顺序检查：

1. BQ27220 Voltage 与万用表是否一致。
2. BQ27220 Current 与实际电流是否一致，方向是否正确。
3. CC Offset、Board Offset、CC Gain、CC Delta 是否校准。
4. 实际 CEDV Config 中 EDV_CMP 是 0 还是 1。
5. 实际 Fixed EDV0/1/2 是否等于目标值。
6. Battery Low 是否为 700。
7. 当前 Battery ID/Profile 是否正确。
8. Design Capacity、Qmax、Learned FCC 是否合理。
9. DOD at EDV2 是否与 Battery Low 同步。
10. 满充后 FC、TCA 是否置位。
11. 放电期间 VDQ 是否有效，EDV2 在什么电压触发。
12. 系统真实稳定截止电压是多少。

## 15. 官方资料

- [BQ27220 Technical Reference Manual](https://www.ti.com/lit/ug/sluubd4a/sluubd4a.pdf)
- [BQ27220 Calibration Guide](https://www.ti.com/lit/an/slua771/slua771.pdf)
- [BQ27220 产品页面](https://www.ti.com/product/BQ27220)
- [TI GPCCEDV 参数计算工具](https://www.ti.com/tool/GPCCEDV)

## 16. 项目源码索引

- `configs/Fightpad12Slim/BoardConfig.h`：硬件引脚和电池目标参数。
- `headers/addons/fightpad_bq27220_battery.h`：驱动接口、默认值和错误状态。
- `src/addons/fightpad_bq27220_battery.cpp`：软件 I2C、运行时读取和 Data Memory 配置。
- `src/gp2040aux.cpp`：BQ27220 Addon 在 Core1 的注册入口。
- `src/display/ui/screens/ButtonLayoutScreen.cpp`：OLED 电池诊断显示。
- `.emv2/discussion/2026-07-09-bq27220-battery-soc-fcc-stability/`：本项目的审计和测试讨论记录。

----

CC Delta ≈ CC Gain × 1,193,046(TI算法使用的固定时间基准/内部数值换算系数)
0.238 × 1,193,046 ≈ 283945

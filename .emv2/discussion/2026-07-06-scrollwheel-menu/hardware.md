# 硬件对齐

## GPIO30-32 现状分析

### BoardConfig.h 定义
| GPIO | 宏定义 | DIP功能 |
|------|--------|---------|
| 30 | FIGHTPAD12SLIM_AMBIENT_ONOFF_PIN | 环境灯开关 |
| 31 | FIGHTPAD12SLIM_AMBIENT_PREV_PIN | 上一个效果 |
| 32 | FIGHTPAD12SLIM_AMBIENT_NEXT_PIN | 下一个效果 |

- 有效电平: `FIGHTPAD12SLIM_AMBIENT_CONTROLS_ACTIVE_LOW 1` (低有效)
- GPIO初始化: `gpio_init` + `GPIO_IN` + `gpio_pull_up`
- 去抖: `CONTROL_DEBOUNCE_MS = 180`

### FightpadAmbientLEDAddon 控制逻辑
- `readControls()`: 读取GPIO引脚值, 低电平=按下
- `handleControlEdges()`: 边沿检测 + 去抖
- 非DIP_SELECTOR_MODE: ONOFF→开关, PREV→上翻, NEXT→下翻

## 拨轮编码器硬件规格
| 信号 | GPIO | 编码器引脚 |
|------|------|-----------|
| SW (按键) | GP30 | 编码器按键 |
| A相 (CLK) | GP31 | 编码器旋转A |
| B相 (DT) | GP32 | 编码器旋转B |

- 类型: 增量式旋转编码器 (正交输出)
- 有效电平: ACTIVE_LOW (同DIP)
- 上拉: GPIO内部上拉已配置

## 引脚冲突检查
| GPIO | 其他复用 | 冲突 | 解决方案 |
|------|---------|------|---------|
| 30 | ASSIGNED_TO_ADDON, AMBIENT_ONOFF | 共享 | 模式仲裁 |
| 31 | ASSIGNED_TO_ADDON, AMBIENT_PREV | 共享 | 模式仲裁 |
| 32 | ASSIGNED_TO_ADDON, AMBIENT_NEXT | 共享 | 模式仲裁 |

**结论**: GPIO30-32无其他外设冲突，仅与FightpadAmbientLEDAddon共享。通过模式管理器 (模块B) 仲裁即可，无需修改硬件配置。

## OLED显示屏
| 项目 | 配置 |
|------|------|
| 接口 | I2C0 |
| SDA | GP0 |
| SCL | GP1 |
| 分辨率 | 128x64 |
| 地址 | 0x3C |

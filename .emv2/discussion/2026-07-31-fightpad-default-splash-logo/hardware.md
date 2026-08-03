# 硬件对齐

讨论ID：`2026-07-31-fightpad-default-splash-logo`

## 现有硬件

- 显示器：128×64 单色 OLED。
- 接口：I2C0。
- 引脚：GP0=SDA、GP1=SCL。

## 本次影响

- 不增加或修改任何 GPIO。
- 不修改 OLED 驱动、I2C 参数、分辨率或像素解码逻辑。
- 只替换 Fightpad12Slim 板级配置初始化时使用的 1024 字节默认启动图。

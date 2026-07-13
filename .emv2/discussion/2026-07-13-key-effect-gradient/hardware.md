# 硬件对齐: Key Effect Gradient

讨论ID: `2026-07-13-key-effect-gradient`

## 已有硬件

- GP22: 现有 12 颗按键 WS2812 LED 链，由 `FightpadAmbientLEDAddon` 渲染和写出。
- GP40: 现有 19 颗 Base WS2812 LED 链，本功能不改变其硬件或渲染选择。
- GP30/GP31/GP32: 现有菜单输入，本功能不改变按键或拨轮处理。

## 配置结论

- 不新增 GPIO、通信接口、供电或电平转换。
- 不改变 PIO、LED 数量、LED 格式、中断或 DMA 配置。
- 继续使用现有 GP22 帧缓冲与 Key Flash 覆盖路径。
- `FIGHTPAD12SLIM_AMBIENT_OWNS_GP22` 保证 GP22 由 Fightpad RGB 路径统一写出。
- 未发现新增引脚或外设资源冲突。

## 用户确认

用户输入 `继续`，确认硬件对齐通过。

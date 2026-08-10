# 硬件对齐

## 已有硬件

- GP30：拨轮 SW 按键，数字输入，低电平有效，使用现有上拉和轮询消抖。
- GP22：12 颗 Key WS2812 灯链的数据输出。
- GP40：19 颗 Base WS2812 灯链的数据输出。
- GP24：RGB 5V 电源使能，沿用 `FightpadAmbientLEDAddon` 的唯一写入和先黑帧后断电路径。

## 新增硬件需求

- 无新增 GPIO。
- 无新增中断、DMA、定时器或通信外设。
- 不修改 RP2350B 与 ESP32-C6 的引脚和 UART 协议。

## 冲突结论

- GP30 继续由当前 `ScrollWheelMenuAddon` 状态机统一判定短按和长按，不增加第二个输入消费者。
- GP22、GP40、GP24 继续由 `FightpadAmbientLEDAddon` 统一输出，避免多写入者冲突。

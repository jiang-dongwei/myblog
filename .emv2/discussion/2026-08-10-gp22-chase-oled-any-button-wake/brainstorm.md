# 头脑风暴

## 采用方案

- GP22周期直接调整`FIGHTPAD12SLIM_GP22_LIGHT_CHASE_CYCLE_MS`，继续使用绝对时间相位公式，避免累计减速。
- 在Core0的ScrollWheelMenuAddon运行路径读取现有已消抖GP2～GP20掩码；任意按键按下时更新已有原子活动时间戳。
- Core1 DisplayAddon继续只读取原子时间戳并控制OLED供电，维持原有跨核所有权。

## 未采用方案

- 不在DisplayAddon新增原始GPIO轮询：会破坏原先“Core0发布、Core1消费”的跨核边界。
- 不只依赖`gamepad->state.buttons/dpad`：Turbo等物理按键未必表现为普通HID按钮，不能保证“任意GP2～GP20按键”。
- 不修改通用WebConfig Display Saver配置：Fightpad板级60秒硬件休眠逻辑已经独立存在。

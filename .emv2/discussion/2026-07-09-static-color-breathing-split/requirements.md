# Static Color 与 Breathing 拆分需求

## 需求

- Base Effect -> Static Color: 选择颜色后固定显示该颜色，亮度不再呼吸变化。
- Key Effect -> Static Color: 选择颜色后固定显示该颜色，亮度不再呼吸变化。
- Key Effect: 新增 Breathing 选项，承接原 Static Color 的呼吸灯行为。

## 约束

- 不改变硬件引脚、PIO、LED 数量。
- 不破坏已有 Key Effect 索引 `0..3` 的含义。
- Chase、Rainbow、Static Theme 行为保持不变。

# Breathing Rainbow 与 Breathing Color 菜单拆分需求

## 需求

- Base Effect 中同时提供自动变色呼吸与单色可选呼吸。
- Key Effect 中同时提供自动变色呼吸与单色可选呼吸。
- 自动变色呼吸命名为 `Breathing Rainbow`。
- 单色可选呼吸命名为 `Breathing Color`，选中后进入颜色菜单。

## 约束

- 不改变 GP22/GP40 引脚、PIO、LED 数量。
- Static Color 保持固定颜色固定亮度。
- 已有 Static/Rainbow/Chase/Static Theme 索引尽量保持不变。

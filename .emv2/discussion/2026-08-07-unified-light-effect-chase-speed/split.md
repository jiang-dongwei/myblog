# 需求拆分

讨论ID：`2026-08-07-unified-light-effect-chase-speed`

## 1. RGB 菜单合并

- 类型：菜单/配置。
- 简述：将 `Key Effect` 与 `Base Effect` 合并为一个 `Light Effect`，保留 `Key Flash`、`Brightness` 和 `All OFF`。

## 2. 双灯链效果同步

- 类型：灯效/状态机。
- 简述：GP22 Key 与 GP40 Base 共享效果模式、选色、动画相位、色轮和 Chase 步进；灯珠数量与原有亮度梯度仍按各自硬件保持。

## 3. Chase 速度微调

- 类型：时序。
- 简述：普通菜单 Chase 从 `200 ms/格` 提升为 `160 ms/格`；蓝牙 Pairing/Connecting 的 `50 ms/格` 临时 Chase 不变。

## 4. 配置兼容和优先级隔离

- 类型：兼容性。
- 简述：不修改 protobuf 字段号；统一效果保存时映射回原 Key/Base 两个字段，并保持 Key Flash、All OFF、GP24 门控、蓝牙覆盖和低电保护行为。

## 已确认约束

- 用户已明确要求合并 Key/Base 灯效并同步，同时略微加快 Chase。
- “略微加快”按 `200 ms -> 160 ms` 实施，即步进间隔缩短 20%。
- 按仓库约定不运行编译，由用户完成构建、烧录与实机验证。

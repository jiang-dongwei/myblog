# Custom Theme 上移到 Lighting 菜单 - 技术方案

## 菜单表

- 在 `kMenuRgbSub` 增加独立 Custom Theme 叶节点，并从 `kMenuLightEffects` 删除该项。
- 调整 `Turn Lights Off` 索引，避免菜单插入后动作落到错误项目。

## 激活与提示

- 在 RGB_SUB 选择分支中复用原 Custom Theme 启用检查和持久化逻辑。
- 提示超时、短按关闭和 Back 均返回 RGB_SUB 的 Custom Theme 行。

## 状态标记

- RGB_SUB 仅在 Custom Theme 实际运行时把其运行时编号作为 active value。
- Custom Theme 运行时抑制 Button Flash 和标准 Lighting Effect 子页的备用状态标记；不清除这些备用配置。

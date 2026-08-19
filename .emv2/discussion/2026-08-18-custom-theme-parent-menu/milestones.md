# Custom Theme 上移到 Lighting 菜单 - 子流程拆分

## S51-A 菜单重排与选择路径

- 将 Custom Theme 移到 RGB_SUB。
- 修正 All Off 索引以及未定义提示的返回层级。

## S51-B 状态标记语义

- 父级显示 Custom Theme 当前标记。
- Custom Theme 运行时隐藏 Button Flash 和标准 Lighting Effect 的备用标记。

## S51-C 静态与实机验证

- 静态检查菜单数量、索引、提示返回、持久化编号及变更范围。
- 用户构建烧录后验证菜单层级、星号和原灯效回归。

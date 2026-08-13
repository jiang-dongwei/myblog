# 子流程拆分

## S42-A：发布当前已确认 BLE Profile

- 将已消抖 BT 挡位与 `FA` ACK 确认结果发布为 Core1 可安全读取的快照。

## S42-B：主页面统一控制器标签

- BT 挡位优先显示确认后的 BLE Profile；USB 挡位显示 USB InputMode。
- BLE Xbox显示`XINPUT`、BLE PS5显示`PS5`；USB标签保留上游原始规则。

## S42-C：静态验证

- 检查 ACK 前回退、ACK 后切换、USB/BT隔离及 `git diff --check`。
- 按仓库约定不运行 RP2350 编译。

## S42-D：构建烧录与实机验证

- 由用户构建、烧录并验证 USB/BT 挡位下 Xbox、PS5 标签和输入历史名称。

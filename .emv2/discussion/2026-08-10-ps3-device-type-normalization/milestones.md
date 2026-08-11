# PS3残留设备类型容错 - 子流程

## S39-A 菜单配置归一

- Controller Type选择时同时保存`inputMode`与普通`GAMEPAD`设备类型。
- 当前模式相同但设备类型不兼容时仍执行保存、黑屏提示和重启。

## S39-B PS3驱动兜底

- PS3初始化只接受Gamepad、Gamepad Alternate、Wheel、Guitar和Drum。
- 其他残留值按普通Gamepad处理。

## S39-C 静态验证

- 检查PS3描述符与报告路径使用归一后的本地设备类型。
- 执行定向搜索和`git diff --check`，不运行编译。

## S39-D 实机验证

- 用户构建并烧录UF2。
- 验证PS3按键输入，并回归Switch、PS4、PS5和Xbox。

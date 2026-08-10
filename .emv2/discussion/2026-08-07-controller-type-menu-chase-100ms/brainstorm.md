# 方案推演

## 方案 A：直接复用上游 InputMode（采用）

- 在拨轮主菜单的 `RGB Customize` 后增加 `Controller Type`。
- 菜单项的 `targetIndex` 直接保存上游 `InputMode` 枚举值。
- 读取 `GamepadOptions.inputMode` 标记当前项；仅当选择发生变化时写入配置，并触发强制保存和重启。
- 优点：不新增协议、不维护第二份模式编号，Web Config、启动按键和拨轮菜单使用同一份配置。

## 方案 B：自定义 Arcade / DInput 别名（不采用）

- 将 Generic HID 显示成 DInput，或增加独立 Arcade 项。
- 问题：名称与上游能力边界不一致；Arcade Stick 在上游是设备类型，不是独立 USB 输入模式，容易让用户误以为存在新的驱动协议。

## 方案 C：选择后只修改运行态（不采用）

- 不保存、不重启，尝试运行中切换驱动。
- 问题：USB 描述符需要重新枚举；运行中直接切换不能可靠地让主机加载新设备类型。

## 最终方案

采用方案 A。列表固定为 XBOX、PS3、PS4、PS5、SWITCH、SWITCH PRO、KEYBOARD、GENERIC HID；没有独立 Arcade。相同模式只刷新显示，不写 Flash；不同模式写入并通过现有存储事件重启。

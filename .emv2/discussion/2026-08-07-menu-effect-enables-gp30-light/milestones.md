# 实施里程碑

## S33-A 非 OFF 菜单选择同步 GP30 ON（completed）

- Key Flash 非 OFF 颜色、Static/Breathing 非 OFF 颜色和三种动态效果统一恢复手动灯光状态。
- 使用现有持久化路径保存。

## S33-B 状态与保存路径静态验证（completed）

- 静态检查状态写入发生在保存事件之前，OFF 分支未被误改。

## S33-C 构建烧录与实机验证（pending）

- 由用户构建烧录，验证 GP30 关闭后从菜单选择非 OFF 效果可立即恢复灯光并在重启后保持。

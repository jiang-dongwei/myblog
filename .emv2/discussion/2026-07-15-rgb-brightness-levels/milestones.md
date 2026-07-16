# 子流程拆分

讨论ID：`2026-07-15-rgb-brightness-levels`
步骤编号：`S20`

## S20-A：亮度配置与持久化

- 在 `FightpadAmbientLEDOptions` 增加亮度档位字段，保持已有字段编号不变。
- 将未设置和越界值归一为 `Bright`。
- 增加运行时共享档位以及启动恢复、菜单保存逻辑。
- 验证：旧配置缺少字段时使用 `0.5f`。

## S20-B：菜单与 OLED 显示

- 在 `RGB Customize` 增加 `Brightness`。
- 增加 Bright、Normal、Dim 列表及当前档位 `*` 标记。
- 实现短按应用并停留、GP19 返回。
- 使用命名索引修复新增菜单项造成的 `All OFF` 位移。
- 验证：菜单表、计数、显示、选择和返回路径完整。

## S20-C：指定灯效亮度控制

- 增加档位到浮点亮度的集中转换。
- 仅修改 Key/Base 的 Static Color、Gradient、Rainbow 六个渲染分支。
- 保持 Key Flash、Chase、Breathing、All OFF 和低电保护不变。
- 验证：目标分支使用新亮度，非目标分支无行为变化。

## S20-D：静态验证

- 定向搜索配置、菜单和渲染路径。
- 检查 `All OFF` 索引和亮度字段边界处理。
- 执行 `git diff --check`。
- 按项目约定不执行编译。

## S20-E：实机验证

- 用户编译烧录后验证三档实时变化和重启恢复。
- 验证 Key Flash 仍为 `0.8f`，Chase/Breathing 不受影响。
- 验证 All OFF 和 7% 低电关灯正常。

## 用户确认

- 用户确认上述子步骤、优先级和验证方式，讨论完成，可以进入开发。


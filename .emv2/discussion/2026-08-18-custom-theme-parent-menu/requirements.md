# Custom Theme 上移到 Lighting 菜单 - 需求确认

## 菜单结构

- `Custom Theme` 不再是 `Lighting Effect` 的选项。
- `Custom Theme` 与 `Button Flash`、`Lighting Effect` 同级。

## 星号规则

- Custom Theme 激活时，`Lighting > Custom Theme` 显示 `*`。
- 此时进入 `Lighting Effect` 不显示任何标准效果的 `*`。
- 此时进入 `Button Flash` 不显示保存的备用闪光颜色 `*`。
- 切换到普通 Lighting Effect 后，原 Button Flash 配置重新生效并恢复正常标记。

## 兼容规则

- 未定义时仍显示约1.5秒 `Custom Theme / Not Defined`，随后返回 `Lighting` 并选中 `Custom Theme`。
- Web Config 关闭已运行主题时继续维持当前主题和父级 `*`，直到玩家切换到其他效果。
- `LIGHT_EFFECT_CUSTOM_THEME`、旧字段映射值7及全部已保存配置不迁移。

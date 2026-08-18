# Custom Theme Lighting Menu - 需求拆分

## 1. Web Config 自定义主题数据

- 类型：配置与存储
- 复用现有 `AnimationOptions.hasCustomTheme` 以及各按键 normal/pressed 颜色。
- 不新增另一份自定义颜色配置，避免 Web Config 与设备菜单产生两个数据源。

## 2. Lighting Effect 菜单

- 类型：显示与状态机
- 在现有五种灯效后增加 `Custom Theme`。
- `Custom Theme` 始终显示在列表中。
- 未定义主题时，选择该项只显示 `Custom Theme / Not Defined` 提示页。
- 无效选择不得改变当前灯效、Flash配置或持久化配置，返回列表后 `*` 仍标记原有效果。

## 3. GP22 按键灯

- 类型：灯光控制
- 将 Web Config 的按键 normal/pressed 颜色映射到 Fightpad12Slim 的12颗物理按键灯。
- 有效选择后由现有 `FightpadAmbientLEDAddon` 渲染，不恢复上游 NeoPico 对 GP22 的写入权。

## 4. GP40 环境灯

- 类型：灯光控制
- 为19颗非按键环境灯定义与12按键自定义主题配合的显示规则。
- 具体映射在需求确认阶段决定。

## 5. 选择与配置持久化

- 类型：状态与存储
- 只有有效自定义主题才能成为当前灯效并获得 `*`。
- 使用现有 Fightpad 灯效配置字段保存新效果标识，避免无必要地改变配置消息结构。
- 必须保留旧配置的兼容性，不能把已有灯效值重新解释成 Custom Theme。

## 6. 异常与回退

- 类型：可靠性
- 未定义主题时仅提示，不自动回退到 Static Color。
- 不清除、不覆盖玩家此前选中的有效灯效。
- 启动时遇到无效的 Custom Theme 持久化状态，需要在后续需求阶段确认显示策略。


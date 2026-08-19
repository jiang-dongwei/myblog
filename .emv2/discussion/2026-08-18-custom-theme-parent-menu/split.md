# Custom Theme 上移到 Lighting 菜单 - 需求拆分

## 1. 菜单层级

- 将 `Custom Theme` 从 `Lighting Effect` 列表移到 `Lighting` 的直接下一级。
- `Lighting` 顺序调整为 `Button Flash / Lighting Effect / Custom Theme / Brightness / Turn Lights Off`。

## 2. 当前状态显示

- Custom Theme 运行时只在 `Lighting` 页的 `Custom Theme` 行显示 `*`。
- `Lighting Effect` 和 `Button Flash` 子页不显示会被 Custom Theme 覆盖的旧状态标记。

## 3. 行为兼容

- 保留 Custom Theme 的运行时编号、持久化值、Web Config 启用检查、未定义提示和延迟停用规则。
- 不修改灯效渲染、硬件引脚、Web API、USB、BLE 或 UART。

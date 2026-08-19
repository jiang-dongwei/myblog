# Web Config Button预览B1直接返回 - 需求拆分

- Button预览入口保持`CONFIG_INSTRUCTION`中B1进入`BUTTONS`。
- Button预览内部B1改为专用返回键，不绘制B1动画。
- 其他按键继续更新Button布局和输入历史。
- 返回后排空同一次物理按键，禁止重新进入页面。

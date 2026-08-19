# Web Config Button预览B1直接返回 - 子流程

## S50-A：直接返回

- 检测B1按下边沿并在旧页绘制前切换到`CONFIG_INSTRUCTION`。

## S50-B：入口释放门控

- ConfigScreen入口等待本次按钮输入释放，防止同一次B1重新进入。

## S50-C：静态验证

- 修复EventManager注销回调循环错误递增外层迭代器导致的卡死。
- 验证Web Config条件隔离、B1无动画和其他按键预览不变。

## S50-D：用户实机验证

- 用户构建烧录后验证B1返回、无卡死、无重复进入及其他按键动画。

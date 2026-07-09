# 需求拆分: Base Chase 平滑亮度梯度

讨论ID: `2026-07-09-base-chase-smooth-gradient`

## 需求拆分

根据需求描述，分析出以下子系统/模块：

### 1. Base Effect Chase 渲染逻辑
- 类型：显示 / 控制
- 简述：修改 `Base Effect -> Chase`，也就是 GP40 底部/环境灯链的 Chase 渲染。

### 2. Chase 亮度梯度
- 类型：算法
- 简述：追逐头部由多个连续灯组成，首尾亮度低，中间亮度高，形成更平滑的视觉拖尾。

### 3. Button Effect Chase 隔离
- 类型：兼容性
- 简述：本需求只改 `Base Effect`，不影响 `Button Effect -> Chase`，除非用户希望两边都同步改。

### 4. 回归验证
- 类型：验证
- 简述：确认 Base Chase 更顺滑，同时 Static Color、RGB OFF、Button Chase 不被破坏。

## 用户确认

用户输入 `继续`，确认需求拆分通过。

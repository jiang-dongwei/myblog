# 记忆日志

## 2026-07-06: 拨轮开关菜单系统

### 决策记录

1. **GPIO30-32共享方案**: 使用B方案（独立GPAddon + 全局标志仲裁），而非扩展现有DisplayMode枚举
   - 原因: 避免侵入现有显示系统，保持DIP功能独立性
   - 仲裁方式: `volatile bool g_scrollWheelMenuActive`

2. **菜单架构**: 独立`ScrollWheelMenuAddon` (GPAddon)，直接调GPGFX渲染，不创建GPScreen子类
   - 原因: 导航方式完全不同（编码器 vs 手柄按键），不需要现有屏幕生命周期

3. **GPIO冲突解决**: 复用共存的方案 — 正常模式DIP生效，菜单模式DIP暂停，退出后恢复
   - `FightpadAmbientLEDAddon::process()` 加一行检查即可

### 关键约束

- 语言: 英文
- 不显示顶部标题栏
- 颜色控制功能暂不开发（仅显示色块列表）
- GP30长按3s进入菜单，长按3s退出（返回BUTTONS）
- 叶子节点短按GP30返回上一层（不到BUTTONS）
- RP2350B/ESP32C6信息页标注"Coming soon"

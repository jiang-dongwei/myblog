# 子流程拆分

## S1-A: 拨轮编码器输入驱动
- **所属子系统**: 模块A
- **开发内容**:
  1. 创建 `headers/addons/scrollwheel_menu.h` 头文件
  2. 实现正交编码器解码 (GP31=A, GP32=B) → delta值
  3. 实现 GP30 长短按检测 (短<3s, 长≥3s)
  4. 去抖处理
- **前置条件**: 无
- **验证方式**: 串口打印旋转方向和按键事件
- **优先级**: 1

## S1-B: 菜单数据模型 + OLED渲染
- **所属子系统**: 模块C
- **开发内容**:
  1. 定义菜单树数据结构 (层级0→1→2 共7个节点)
  2. 实现菜单渲染函数 (列表 + 高亮 + 滚动指针)
  3. 实现导航逻辑 (编码器旋转=上下, 短按=进入/返回)
  4. 英文菜单字符串
- **前置条件**: S1-A
- **验证方式**: 固定模式=菜单时，OLED显示完整菜单层级
- **优先级**: 2

## S1-C: FightpadAmbientLEDAddon 修改
- **所属子系统**: 模块D
- **开发内容**:
  1. 添加全局标志 `g_scrollWheelMenuActive`
  2. 在 `FightpadAmbientLEDAddon::process()` 中: 菜单激活时跳过 `handleControlEdges()`
- **前置条件**: 无
- **验证方式**: 菜单激活时 DIP 按键不触发 LED 效果切换
- **优先级**: 1 (可与S1-A并行)

## S1-D: 模式管理器 + ScrollWheelMenuAddon 集成
- **所属子系统**: 模块B + 集成
- **开发内容**:
  1. 创建 `ScrollWheelMenuAddon` GPAddon 骨架
  2. 实现 MODE_NORMAL ↔ MODE_MENU 状态切换
  3. 菜单激活时: 阻止 DIP, 渲染菜单到OLED
  4. 菜单退出时: 恢复 DIP, 显示回到 BUTTONS
  5. 注册 addon 到 CMakeLists.txt + addonmanager
- **前置条件**: S1-A, S1-B, S1-C
- **验证方式**: 完整交互流程 — 长按进菜单 → 旋转导航 → 短按选择 → 长按退出
- **优先级**: 3

## S1-E: 编译验证 + 固件烧录
- **所属子系统**: 集成
- **开发内容**:
  1. 添加 `scrollwheel_menu.cpp` 到 CMakeLists.txt
  2. 编译验证
  3. 烧录测试
- **前置条件**: S1-D
- **验证方式**: 设备上实际测试所有交互路径
- **优先级**: 4

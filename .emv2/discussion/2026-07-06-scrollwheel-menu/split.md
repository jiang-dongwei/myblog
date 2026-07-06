# 需求拆分

## 讨论ID: 2026-07-06-scrollwheel-menu

## 子系统列表

### 模块A: 拨轮输入驱动
- 类型: 测量
- 简述: 读取拨轮编码器(GP31=A相, GP32=B相)旋转方向/步数，检测GP30按键短按/长按(3s)

### 模块B: 模式管理器 (核心状态机)
- 类型: 状态机
- 简述: 管理"正常模式(DIP生效)"↔"菜单模式(菜单操作)"切换；仲裁GPIO30-32归属

### 模块C: OLED多层级菜单
- 类型: 显示
- 简述: 128x64 OLED上渲染3级英文菜单

### 模块D: GPIO共享仲裁 (FightpadAmbientLEDAddon扩展)
- 类型: 控制
- 简述: 正常模式下DIP控制环境灯；进入菜单后DIP暂停，退出后恢复

## 交互规则

- GP30长按3s: 进入菜单 (从BUTTONS) / 返回BUTTONS (从任意菜单层级)
- GP30短按: 有子菜单→进入下一层；无子菜单→返回上一层(不到BUTTONS)
- 拨轮旋转: 菜单项上下滚动

## 菜单层级

```
Level 0: Main Menu
  1. RP2350B Firmware Version → Level 1a
  2. ESP32C6 Status → Level 1b
  3. RGB Customize → Level 1c

Level 1a/b: Info pages (Coming soon)
Level 1c: RGB Customize
  3.1 Top Board RGB → Level 2
  3.2 Bottom Board RGB → Level 2
  3.3 Button RGB → Level 2

Level 2: Color Select (暂不开发颜色控制, 仅显示)
  Red/Orange/Yellow/Green/Cyan/Blue/Purple/White
```

## GPIO仲裁方案

- 正常模式: DIP控制环境灯 (GP30=ON/OFF, GP31=PREV, GP32=NEXT)
- 菜单模式: DIP暂停, GPIO30-32归菜单使用
- 退出菜单: 恢复DIP功能

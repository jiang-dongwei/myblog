# 需求确认

## 模块A: 拨轮输入驱动

- 编码器类型: 旋转编码器 (A/B相正交) + 按键(GP30)
- 旋转检测: A/B相跳变沿检测
- 按键检测: 短按(<3s) / 长按(≥3s)
- 去抖周期: ~50ms(旋转), ~80ms(按键)
- 有效电平: ACTIVE_LOW (同DIP)
- 上拉: GPIO内部上拉

## 模块B: 模式管理器

- 状态数: 2 (MODE_NORMAL / MODE_MENU)
- 进入触发: MODE_NORMAL + GP30持续按下≥3000ms
- 退出触发: MODE_MENU + GP30持续按下≥3000ms
- 初始状态: MODE_NORMAL

## 模块C: OLED菜单

- 显示区域: 128x64 全屏
- 字体: 现有GPGFX字体
- 每页菜单项数: ~4行
- 滚动支持: 需要
- 选中高亮: 反色/箭头指示
- 语言: 英文
- 不含顶部标题栏
- 颜色页: 仅显示色块+名称, 暂不写控制

## 模块D: GPIO仲裁

- 仲裁者: 模式管理器通知FightpadAmbientLEDAddon
- DIP暂停: 设置enabled=false跳过process()
- DIP恢复: 退出菜单后恢复

# 需求拆分: Turn Lights Off可恢复总开关

讨论ID: `2026-08-19-persistent-all-lights-toggle`

## 1. 灯光总开关存储

- 类型: 存储/状态机
- 简述: 新增独立持久化总开关，不再用黑色和未设置灯效表示关闭。

## 2. 全LED输出门控

- 类型: 控制
- 简述: 总开关关闭时同时禁止GP22、GP40、Button Flash和蓝牙连接状态灯输出。

## 3. 菜单切换反馈

- 类型: 显示
- 简述: 同一菜单项在开启时显示Turn Lights Off，关闭时显示Turn Lights On。

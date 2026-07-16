# 方案

- 不删除 `SWMenuLevel::BATTERY_INFO` 或四页绘制函数。
- 使用 `SCROLLWHEEL_BATTERY_INFO_MENU_ENABLED` 只控制 `kMenuMain` 中的入口。
- Fightpad12Slim 默认设为 `0`；调试时改成 `1` 即可恢复入口。
- 主菜单项数量继续由 `sizeof(kMenuMain)` 自动计算，不增加固定索引依赖。


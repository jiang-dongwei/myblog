# 需求确认: FightpadSlim USB产品名

讨论ID: `2026-08-19-fightpadslim-usb-product-name`

## 已确认行为

1. Fightpad12Slim在USB传输档位下向主机报告产品名`FIGHTPADSLIM`。
2. PS4、PS3、Generic、Keyboard等有线控制器模式使用同一产品名。
3. 保持各模式原VID/PID、协议描述符和输入行为不变。
4. 不修改ESP32-C6蓝牙广播名称。
5. 烧录后需要断开并重新连接USB，让主机重新枚举；若Windows仍显示旧名，需要移除缓存设备后再连接。

## 用户确认

用户根据主机识别截图明确要求将USB模式中显示的GP2040名称改为`FIGHTPADSLIM`；按简单功能采用需求确认后直接开发流程。

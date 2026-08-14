# 需求确认

- Xbox Profile PnP：USB source，VID `045E`，PID `0B13`，版本 `0509`。
- 序列号：采用参考实现的 1914 序列号
  `3039373130303637313034303231`。
- HID Report Map：逐字节采用本地 MIT 参考工程的 1914 描述符。
- Input Report 1：16 字节，保持现有按键、方向、摇杆和数字扳机映射。
- Output Report 3：保留 GATT 特征供主机识别，但固件忽略输出，不支持振动。
- 蓝牙广播名称保持 `FP12Slim-C6`，不靠改名伪装 Xbox。
- 普通开机不得清 bond 或自动进入配对；切换 Profile 的现有重启与修复逻辑不回退。
- Windows 必须删除旧缓存设备后重新配对，才能评价新 Profile 的驱动选择。

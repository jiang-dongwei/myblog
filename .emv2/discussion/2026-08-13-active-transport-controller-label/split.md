# 需求拆分

1. 通信状态：复用现有 ESP32-C6 `FA` Profile ACK，不新增串口帧。
2. 传输状态：复用 FightpadESP32ProxyAddon 已消抖的 USB/BT 挡位。
3. 主页面显示：按当前传输源选择已生效的控制器类型标签。


# 头脑风暴

## 难点 1：ESP32-C6 不支持经典蓝牙

- 结论：不尝试旧 Xbox Classic Bluetooth 路径；采用 Xbox Series X|S 1914
  的原生 BLE Profile，协议承载仍是标准 HOGP。

## 难点 2：Windows 是否选择 XInput 驱动

- 方案：同时对齐 PnP 身份、序列号、完整 Report Map 和 Report 特征集合。
- 风险：浏览器显示由 Windows 驱动路径决定，源码静态一致不能替代实机结果。
- 验证：Windows 删除旧设备、C6 进入修复配对后重新枚举，检查设备管理器是否出现
  `Bluetooth LE XINPUT compatible input device`，再检查 Gamepad API。

## 难点 3：不能为测试破坏重连

- 方案：不提升 NVS 版本、不在普通开机清 bond；由用户执行一次明确的旧设备删除与
  重新配对。这样后续启动仍走原绑定设备重连。

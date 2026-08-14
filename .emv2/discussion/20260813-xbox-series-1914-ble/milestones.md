# 子流程拆分

## S6-A：1914 Profile 对齐

- 替换 Xbox PnP、序列号和逐字节 Report Map。
- 将 NimBLE Report 特征最低容量约束从 1708 的四个调整为 1914 的两个。

## S6-B：回归测试

- 更新 Profile 身份、描述符长度、Report 1/3 特征和固定输入向量测试。
- 运行所有主机测试和 ESP32-C6 编译。

## S6-C：实机验收

- Windows 删除旧设备并重新配对。
- 检查配对、重连、设备管理器驱动和网页 `Gamepad.id/mapping`。

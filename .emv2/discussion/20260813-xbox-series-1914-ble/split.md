# 需求拆分

## 1. BLE HID Profile

- 类型：通信/兼容性
- 将现有 Xbox One S 1708 BLE HOGP Profile 替换为上游已验证过 Windows
  XInput 驱动路径的 Xbox Series X|S 1914 Profile。

## 2. 输入报告编码

- 类型：数据映射
- 保持 RP2350 USB XInput 语义及现有 16 字节 Report 1 编码，不实现振动。

## 3. 配对、重连与持久化保护

- 类型：状态机/存储
- 不改变 GPIO13 配对、bond 保护、定向重连、UART0 协议和双模式持久化。

## 4. 验证

- 类型：测试
- 主机测试验证身份、描述符和输入向量；ESP32-C6 编译验证集成；Windows
  驱动选择和网页识别留给实机验收。

# 需求确认: ESP32-C6 固件信息串口接收与菜单显示

讨论ID: `2026-07-13-esp32-firmware-info-uart`

## UART 通信

- 通信对象: RP2350B 与 ESP32-C6。
- 外设与引脚: RP2350B UART0，GP44=TX，GP45=RX。
- 参数: 115200、8N1、无硬件流控。
- 接收方式: 主循环非阻塞轮询，不增加 IRQ 或 DMA。
- 实时性: ESP32-C6 上电时接收一次，属于非实时设备信息。

## 帧与序列规则

- 固件信息帧固定 8 字节，以 `0x46 0x49` 开头。
- 验证 Byte0~6 XOR、FIRST/MIDDLE/LAST/SINGLE 和连续 seq。
- 使用滑动同步过滤 ESP-IDF Console 日志和错误候选帧。
- 帧间超时为 200ms；超时、乱序、坏帧或溢出时丢弃当前序列。
- Payload 最大 256 字节；末尾 `0x00` 填充在解析前去除。

## 字段与缓存

- 解析 `SDK`、`Plat`、`Board`、`CPU`。
- 未知的格式正确字段可以忽略，四个必需字段齐全后才发布。
- 新的无效序列不清除上一份有效信息。
- 信息只保存于 RAM，不写 Flash。

## OLED 页面

有效数据页面使用 8 行字符布局：

```text
ESP32C6 Firmware
SDK: <SDK>
Plat: <Plat>
Board:
<Board 第1段>
<Board 第2段>
CPU: <CPU>
Back: press
```

- 每行最多 21 个字符。
- Board 最多显示两行，每行 21 个字符。
- 未接收到完整有效数据时，页面显示用户指定文本 `Coming to soon`。
- INFO 页保持拨轮禁用和短按返回主菜单的现有行为。

## 用户确认

用户补充并确认无数据文案为 `Coming to soon`，随后输入 `继续` 通过需求确认。

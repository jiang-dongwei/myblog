# GPIO13 Pairing Toggle RP2350 - 需求确认

- ESP32-C6是配对状态权威源，RP2350只显示收到的最新合法状态。
- Pairing由`0x03`表示；取消、失败、连接中或成功分别由后续`0x00/0x01/0x02`终止Pairing。
- 重复状态帧用于链路可靠性，RP2350必须把它们视为同一状态事件。
- 不修改BLE Profile、USB模式、配置存储、Bond、UART协议、ACK或重启逻辑。

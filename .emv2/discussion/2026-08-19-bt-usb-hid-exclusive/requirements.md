# 需求确认: 蓝牙档位 USB HID 互斥

讨论ID: `2026-08-19-bt-usb-hid-exclusive`

## 已确认行为

1. GP33 物理档位继续是传输模式的唯一运行时真值，不因检测到 VBUS 自动切换。
2. 普通游戏模式下，GP33 高电平选择蓝牙：ESP32-C6 保持启用，RP2350 USB Device 关闭 D+/D- 上拉，不向电脑枚举 USB 手柄。
3. 蓝牙档位插入 USB 仍允许硬件供电和充电，固件只关闭 USB 数据枚举。
4. GP33 低电平选择 USB：ESP32-C6 按现有逻辑关闭，RP2350 重新连接并枚举已配置的 USB 控制器模式。
5. Web Config 不受 GP33 蓝牙档位限制，RNDIS/ECM 必须保持可访问。
6. BOOTSEL 不修改，继续在进入 RP2350 ROM USB 启动后工作。
7. 不修改 USB InputMode、BLE Profile、UART 帧、配置存储或 ESP32-C6 工程。

## 用户确认

用户在获知现有“USB保持枚举但中立”会导致游戏选错控制器后，明确回复“好，那你修复吧”，确认采用 USB/BLE 数据通道互斥方案。


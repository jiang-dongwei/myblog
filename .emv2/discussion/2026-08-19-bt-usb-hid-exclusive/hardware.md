# 硬件对齐: 蓝牙档位 USB HID 互斥

讨论ID: `2026-08-19-bt-usb-hid-exclusive`

## 已有硬件

| 信号 | 功能 | 当前规则 |
|---|---|---|
| GP33 | USB/BT 传输档位输入 | 低=USB，高=BT，运行时稳定30ms后生效 |
| GP34 | ESP32-C6 CHIP_PU/EN | USB拉低，BT拉高 |
| USB D+/D- | RP2350 TinyUSB Device | 由 `tud_connect()` / `tud_disconnect()` 控制上拉 |
| USB VBUS | 供电/充电与主机连接 | 不作为传输模式选择条件 |

## 边界

- 不新增引脚，不改变 GP33/GP34 电平极性。
- `tud_disconnect()` 只断开 USB 数据设备，不控制板级 USB 供电和充电路径。
- ESP32-C6 UART、蓝牙广播和 HID Profile 不作修改。


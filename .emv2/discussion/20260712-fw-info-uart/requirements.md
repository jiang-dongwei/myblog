# 需求确认

## 子系统1: 固件信息采集

| 字段 | 值示例 | 获取方式 |
|------|--------|---------|
| SDK | `5.5.1` | `esp_get_idf_version()` 去前缀v |
| Plat | `esp32c6` | `CONFIG_IDF_TARGET` 宏 |
| Board | `fightpad12slim_c6_ble_hid` | CMake传入 `PROJECT_NAME` |
| CPU | `RISC-V` | `CONFIG_IDF_TARGET_ARCH` 映射 |

## 子系统2: UART发送

- 协议: 二进制帧，扩展8字节帧协议
- 帧类型: `I` (0x49) — Firmware Info
- 触发: 启动时发送一次
- 分包: 多帧发送，payload文本格式 `key=value\n`

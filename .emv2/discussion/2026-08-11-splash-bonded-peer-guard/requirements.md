## 需求确认

### RP2350 OLED
- Fightpad12Slim 静态 Splash 保持 3000 ms。
- Splash 期间不渲染 Pairing、Connecting、Connected 或 Disconnected 覆盖页。
- 蓝牙状态接收、跨核快照、OLED 唤醒和 GP40 状态灯逻辑保持不变。
- Splash 结束后，仍有效的持续状态可按原规则显示；已经超时的结果状态不补播。

### ESP32-C6 BLE
- 权威工程路径为 `E:\WorkSpace\C_WorkSpacee\ESP-IDF5.2\.espressif\release-v5.2\esp32c6_ble_hid_gamepad_test`。
- 只有 GPIO13 显式触发的 30 秒窗口允许普通可发现广播和新设备绑定。
- 启动时不自动打开普通配对窗口。
- 有持久化绑定时，启动先对最近绑定身份地址进行高占空比定向广播，随后保持低占空比定向广播。
- 无持久化绑定且未按 GPIO13 时不广播，避免陌生设备抢占。
- 非显式配对连接必须来自定向广播；否则请求断开。
- Repeat Pairing 只在本次连接来自显式配对窗口时允许删除旧键并重试。

用户已回复“好，就这么改”，视为上述参数与行为确认。

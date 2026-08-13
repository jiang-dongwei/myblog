# 需求确认

- USB 挡位：使用 RP2350 当前 USB InputMode。
- BT 挡位：使用 C6 `FA` ACK 确认的 BLE Profile，不使用尚未确认的菜单选择。
- USB Xbox 保留上游原有 `XINPUT`/`XB360` 状态显示；BLE Xbox 使用规范化的 `XINPUT`。
- PS5 无论 USB 或 BLE 都显示 `PS5`。
- Generic 显示 `USBHID`，Keyboard 显示 `HID-KB`。
- 除选择当前传输的数据来源外，不改写上游已有USB标签和认证状态显示。
- 只修改 BUTTONS 主页面状态栏，不联动配置、不新增重启。

# 需求确认: BLE 自动重连

## 子系统 1: BLE 广播状态机

- 修改位置: `start_advertising()` 函数
- 移除 `pairing_window_active()` 条件
- 保留: `s_hid_started` + `transport_bt_enabled()` 条件
- 广播触发时机不变（HID start、断连、ADV_COMPLETE）
- 广播停止时机不变（连接成功 stack 自动停、USB 模式切换）

## 子系统 2: 配对模式与广播分离

- 删除 `pair_button_task` 中配对窗口过期停止广播的代码
- pairing_window 只控制：清 bond 表、允许新设备配对
- pairing_window 不再控制广播启停

## 子系统 3: GPIO13 按键行为

- 行为不变：清 bond + 开窗 + 广播
- 新场景：断电重启后自动回连已配对主机（无需按按键）

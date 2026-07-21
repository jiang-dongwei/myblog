# 子流程拆分: 蓝牙连接状态弹窗与 Base 灯效

讨论ID: `2026-07-20-bluetooth-status-popup-led`
步骤编号: `S23`

## S23-A: UART 状态帧与跨核事件

- 扩展统一 8 字节帧同步、XOR 校验和 `0x49`/`0x53` 类型分发。
- 验证状态码并发布包含接收时间的跨核安全快照。

## S23-B: OLED 临时连接页面

- 状态事件优先于休眠和普通页面绘制。
- Connecting/Pairing 持续显示；Connected/Disconnected 维持 1000ms。
- 覆盖结束后恢复原页面，不改菜单状态。

## S23-C: GP40 临时状态灯效

- Connecting/Pairing 渲染纯蓝 Chase。
- Connected 渲染全蓝静态灯，Disconnected 渲染全黑。
- 支持 All OFF 临时唤醒并保持 7% 低电保护最高优先级。

## S23-D: 静态验证

- 检查三种合法帧、错误校验、非法状态和 `0x49` 回归路径。
- 检查 OLED 期限、GP22 隔离、配置无写入、GP24 与低电优先级。
- 执行 `git diff --check`，不运行编译。

## S23-E: 实机验证

- 用户构建烧录后验证 Pairing -> Connecting -> Connected、Connecting -> Disconnected。
- 验证普通灯效、All OFF、OLED 休眠、菜单页面及低电状态下的恢复行为。

## 用户确认

用户输入“同意，继续”，确认子流程拆分。

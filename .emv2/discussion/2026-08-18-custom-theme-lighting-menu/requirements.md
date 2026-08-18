# Custom Theme Lighting Menu - 需求确认

## 1. 菜单入口

- `Lighting Effect` 增加第六项 `Custom Theme`。
- 该项目始终显示，不根据 `hasCustomTheme` 动态隐藏。
- 星号 `*` 只表示当前实际运行并已保存的灯效。

## 2. 未定义主题时的选择行为

- 用户选择 `Custom Theme` 时检查 Web Config 自定义主题是否已经定义并启用。
- 未定义时显示独立提示页：

  ```text
  Custom Theme
  Not Defined
  ```

- 提示约1.5秒后返回 `Lighting Effect` 列表。
- 不改变当前灯效。
- 不移动 `*`。
- 不改变 Button Flash、亮度、电源请求或其他灯光状态。
- 不写入Flash配置。

## 3. 有效自定义主题

- GP22的12颗按键灯使用 Web Config 中每个逻辑按键的 normal 颜色。
- 按键按下时使用对应逻辑按键的 pressed 颜色。
- `Custom Theme` 运行期间不使用统一 Button Flash 颜色。
- Button Flash 的既有配置必须保留；切换到其他灯效后恢复正常作用。
- GP40使用12个normal颜色的循环平滑插值，扩展为19颗环境灯的静态主题。

## 4. 延迟停用规则

- Web Config 将自定义主题设为不启用时，不清除已经保存的颜色。
- 如果当前灯效不是 `Custom Theme`，后续选择它按“未定义主题”处理。
- 如果当前正在运行 `Custom Theme`：
  - 当前主题继续运行；
  - `*` 继续标记 `Custom Theme`；
  - 重启和重新上电后仍继续显示已保存的自定义主题。
- 当前主题停用后，玩家一旦切换到其他灯效，就不能再次进入 `Custom Theme`，直到 Web Config 重新启用主题。
- Web Config 重新启用后，`Custom Theme` 恢复可选择状态。

## 5. 持久化与兼容性

- 有效选择 `Custom Theme` 后才保存当前效果。
- 使用新的、未占用的 Fightpad 灯效值，不重新解释旧值。
- 不改变 UART、USB、BLE 或 Web API 协议。
- 不改变现有五种灯效及其已保存配置的含义。

## 6. 共同行为

- `Custom Theme` 继续受现有 GP30 灯光总开关、亮度设置、供电门控、低电量处理和蓝牙状态覆盖逻辑约束。
- 蓝牙 Pairing/Connecting 等临时状态结束后，应恢复此前正在运行的 `Custom Theme`。


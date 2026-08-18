# Custom Theme Lighting Menu - 技术方案

## 1. 灯效编号与兼容存储

### 难点

当前菜单使用统一的运行时灯效编号，但Flash仍通过两个旧的 Key/Base 效果字段兼容保存。不能重新解释旧值，否则已有设备升级后会改变灯效。

### 方案

- 新增运行时编号 `LIGHT_EFFECT_CUSTOM_THEME = 5`。
- 将 `LIGHT_EFFECT_COUNT` 调整为6。
- 使用旧字段中未占用值7表示Custom Theme，并为两个方向的转换函数增加明确映射。
- 不修改protobuf字段结构，不改变旧编号含义。

## 2. 延迟停用

### 难点

`hasCustomTheme=false`既要阻止新的选择，又不能中断已经运行并持久化的Custom Theme。

### 方案

- 激活检查：从其他效果进入Custom Theme时必须满足 `hasCustomTheme=true`。
- 运行检查：当前已保存效果为Custom Theme时，无论 `hasCustomTheme` 是否随后关闭，都继续使用已保存颜色渲染。
- 切换到其他效果并保存后，原Custom Theme运行资格自然消失。
- 重启时如果保存效果仍为Custom Theme，则继续渲染已保存颜色。

## 3. OLED未定义提示

### 难点

提示页不能阻塞主循环、灯光刷新或菜单按键处理，也不能错误改变星号状态。

### 方案

- 使用单独的临时提示状态和截止时间，不使用sleep或忙等待。
- 显示 `Custom Theme / Not Defined` 约1.5秒。
- 截止后恢复Lighting Effect列表。
- 无效选择路径不修改 `g_menuLightEffect`、持久化字段、GP30灯光请求或星号依据。

## 4. GP22 Normal/Pressed颜色

### 难点

现有普通Button Flash是按下沿触发的80ms短闪，不等价于Web Config的按住Pressed状态。

### 方案

- Custom Theme根据当前Gamepad buttons/dpad电平直接判断12个逻辑按键是否按住。
- 按住期间持续使用对应Pressed颜色，松开立即恢复Normal颜色。
- 其他五种效果继续使用原80ms Flash逻辑。
- Custom Theme运行时保留但忽略统一Button Flash颜色。

## 5. GP40的19灯主题

### 难点

Web Config仅定义按键颜色，没有19颗环境灯的独立颜色字段。

### 方案

- 按现有12灯物理顺序取得Normal颜色。
- 将12色视为首尾相接的色环。
- 以19个等距采样点进行相邻颜色线性插值。
- 按键Pressed只改变GP22，GP40保持静态主题。

## 6. 驱动和覆盖优先级

- FightpadAmbientLEDAddon继续独占GP22/GP40。
- 不恢复NeoPico对GP22的输出。
- GP30关闭、低电保护和最终黑帧优先于Custom Theme。
- Pairing/Connecting等临时蓝牙状态可以覆盖显示，结束后恢复Custom Theme。
- 亮度继续通过现有共享亮度路径应用。


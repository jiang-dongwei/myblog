# Web Config Button预览B1直接返回 - 需求确认

- 仅在`DriverManager::isConfigMode()`为真时改变行为。
- B1按下立即返回`DisplayMode::CONFIG_INSTRUCTION`。
- B1不参与该页面的按键图案动画。
- 不使用A2返回；其他按键保留原预览行为。
- 正常开机后的`DisplayMode::BUTTONS`中B1仍正常显示。

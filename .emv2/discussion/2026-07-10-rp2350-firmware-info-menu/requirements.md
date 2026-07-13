# 需求确认: RP2350B 固件信息菜单

讨论ID: `2026-07-10-rp2350-firmware-info-menu`

## 显示内容

RP2350B INFO 页使用 8 行字符布局：

```text
RP2350B Firmware
SDK: <PICO_SDK_VERSION_STRING>
Plat: <GP2040PLATFORM>
Board: <GP2040_BOARDCONFIG>
CPU: Cortex-M33
Back: press
```

## 交互约束

- 每行最多 21 个字符，超长构建字符串必须截断。
- INFO 页不响应拨轮旋转。
- 短按返回主菜单。
- 不修改 ESP32 INFO 页、RGB 菜单或菜单层级。

## 用户确认

用户输入 `继续`，确认显示内容和交互约束。

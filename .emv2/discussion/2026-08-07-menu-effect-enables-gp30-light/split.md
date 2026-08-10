# 需求拆分

## 菜单与 GP30 手动灯光状态同步

- 菜单选中可见的非 OFF 灯光颜色或效果时，同时将 `manualLightEffectsEnabled` 设为 `true`。
- `All OFF` 和颜色列表中的 `OFF` 保持关闭语义，不自动打开手动灯光。
- 继续复用现有 protobuf 字段和 `persistConfig()`，不新增配置项。

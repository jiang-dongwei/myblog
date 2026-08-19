# 子流程拆分: Turn Lights Off可恢复总开关

讨论ID: `2026-08-19-persistent-all-lights-toggle`

## S55-A: 持久化总开关

- 所属子系统: Protobuf配置/ScrollWheel菜单
- 开发内容: 新增默认开启字段，加载和保存总开关；旧全黑编码提供安全兜底。
- 验证方式: 静态检查字段编号、默认值及灯效字段不被关闭操作修改。

## S55-B: 全LED硬门控

- 所属子系统: FightpadAmbientLEDAddon
- 开发内容: 总开关同时门控正常灯效、蓝牙状态灯和最终GP24供电。
- 验证方式: 静态追踪render/show路径。

## S55-C: 动态菜单标签

- 所属子系统: OLED菜单
- 开发内容: 根据总开关显示Turn Lights Off或Turn Lights On。
- 验证方式: 静态检查索引和显示分支。

## S55-D: 实机回归

- 所属子系统: 验证
- 开发内容: 用户构建烧录，验证恢复、重启保持、蓝牙状态灯和GP30独立语义。
- 验证方式: OLED与两条LED链实机测试。

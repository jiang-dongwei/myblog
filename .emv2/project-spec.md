# 项目规格单

## 项目信息

| 项目 | 值 |
|------|-----|
| 名称 | Fightpad12Slim RP2350B Firmware |
| 基于 | GP2040-CE v0.7.12 |
| 开发板 | RP2350B (SparkFun Pro Micro RP2350) |
| BoardConfig | Fightpad12Slim |
| 编译目标 | `build/` (rp2350-arm-s) |

## 新增功能模块

### 拨轮开关菜单系统
- 物理硬件: 旋转编码器 (GP30=SW, GP31=A, GP32=B)
- 功能: 长按GP30进入→OLED显示多级菜单→拨轮导航→长按退出
- 讨论ID: `2026-07-06-scrollwheel-menu`

## 开发步骤状态

| 步骤 | 描述 | 状态 | 讨论ID |
|------|------|------|--------|
| S1-A | 拨轮编码器输入驱动 | pending | 2026-07-06-scrollwheel-menu |
| S1-B | 菜单数据模型 + OLED渲染 | pending | 2026-07-06-scrollwheel-menu |
| S1-C | FightpadAmbientLEDAddon GPIO仲裁 | pending | 2026-07-06-scrollwheel-menu |
| S1-D | 模式管理器 + ScrollWheelMenuAddon | pending | 2026-07-06-scrollwheel-menu |
| S1-E | 编译验证 + 固件烧录 | pending | 2026-07-06-scrollwheel-menu |
| S2-A | 修复长按GP30时LED误切换 (删除 g_scrollWheelButtonBusy 抑制) | completed | 2026-07-07-longpress-no-led-toggle |
| S3-A | INFO页禁用拨轮滚动 + COLOR层级短按返回上层 | completed | 2026-07-07-menu-nav-fixes |
| S4-A | 全局颜色状态变量 + COLOR层级短按写入颜色 | completed | 2026-07-07-rgb-color-control |
| S4-B | render()使用菜单颜色覆盖 + 按钮闪灯颜色 | completed | 2026-07-07-rgb-color-control |
| S5-A | RGB_SUB增加"RGB OFF" + COLOR增加"OFF"色 | completed | 2026-07-07-rgb-off |

# 子流程拆分

讨论ID：`2026-07-31-webconfig-fightpad-branding`

步骤编号：`S27`

## S27-A：Fightpad Logo 与导航字标

- 开发内容：新增方框 R SVG，在导航栏组合 `R + FIGHTPAD`，更新提示和替代文本。
- 验证方式：SVG/XML、源字模轮廓和导航组件定向检查。
- 状态：completed。

## S27-B：品牌文案与浏览器元数据

- 开发内容：更新共享品牌名、首页欢迎标题、页面 title/description/favicon 和 manifest。
- 验证方式：所有语言定向搜索、JSON/XML 解析和资源引用检查。
- 状态：completed。

## S27-C：非编译静态与视觉预览

- 开发内容：检查响应式样式、亮暗背景预览和补丁格式。
- 验证方式：临时图片预览、`git diff --check` 和资源一致性脚本；不运行构建。
- 状态：completed。

## S27-D：设备 Web Config 实际页面验证

- 开发内容：用户构建烧录后检查桌面端、移动端、亮色和暗色导航栏。
- 前置条件：S27-A、S27-B、S27-C 完成。
- 状态：pending。

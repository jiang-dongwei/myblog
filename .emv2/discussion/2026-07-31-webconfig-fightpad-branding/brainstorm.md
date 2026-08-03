# 头脑风暴

讨论ID：`2026-07-31-webconfig-fightpad-branding`

## 方案比较

### 直接替换旧 PNG

- 旧资源本身包含完整 `GP2040-CE` 字样，不便单独调整图形和文字。
- 二次缩放会继续依赖大尺寸位图。

### SVG 图形与 HTML 文字分离

- SVG 可精确复用 S26 的 1-bit Logo 轮廓并保持透明背景。
- `FIGHTPAD` 使用真实文字，清晰、可访问，并可用 CSS 独立调整。
- 同一个 SVG 还能复用于浏览器图标和 manifest。

## 决策

采用 SVG 图形与 HTML 文字分离方案；保留旧 PNG 文件但取消页面引用，避免破坏性删除。

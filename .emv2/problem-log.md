# 问题追踪

## 当前问题

### [2026-08-03] 设备 Web Config 仍显示 GP2040 品牌

- **状态**: in_progress
- **步骤**: S27-D
- **发现时间**: 2026-08-03
- **相关HVR**: `.emv2/checkpoints/HVR-S27-001.md`
- **描述**: 首轮实机页面仍显示旧 GP2040 品牌；返工版本文字已更新但 SVG 显示破图；删除图片后重新连接 Web Config，页面仍无变化。

### 分析

- `www/src/Data/Buttons.js` 中模式标签仍为 `GP2040`，这是右上角选项未变化的直接原因。
- `www/build/assets/index-499bbe5c.js`、`www/build/index.html` 和 `www/build/manifest.json` 仍是旧品牌输出。
- 设备直接使用 `lib/httpd/fsdata.c` 中的内嵌 Web 文件；该文件未随本轮 `www/src` 修改重新生成。
- `SKIP_WEBBUILD=TRUE` 会跳过前端构建和 `fsdata.c` 生成，导致新源码没有进入用户烧录的固件。
- 返工截图确认 `FIGHTPAD` 文字已经进入设备页面，当前剩余问题仅来自导航栏 `<img>` 资源加载失败。
- 删除 `<img>` 后复测仍无变化，说明当前设备加载的 Web 数据尚未更新到这次源码修改；正在核对 `www/build`、`fsdata.c` 和最终 UF2 的产物链。
- 时间戳已确认：导航源码为13:35、Web JS/CSS为13:36；`fsdata.c`为11:37:31、httpd对象为11:37:46、最终UF2为11:37:58。
- UF2路径 `build/GP2040-CE_0.0.0_Fightpad12Slim.uf2` 正确，但该文件早于删除图片修改，实机无变化不是路径指向错误。

### 解决方案

- 将模式显示标签改为 `FIGHTPAD`，保留内部值 `gp2040`，避免影响已有配置兼容性。
- 删除导航栏图片节点及 `.title-logo` 样式，只保留文字字标；移除 flex 间隔，让 `FIGHTPAD` 从原 Logo 左边界开始显示。
- 完整执行 `www` 下的 `npm run build`，生成带新内容哈希的 JS/CSS 并同步更新 `lib/httpd/fsdata.c`。
- 随后重新编译 `build/GP2040-CE_0.0.0_Fightpad12Slim.uf2`、烧录，并使用强制刷新或无痕窗口复测，排除旧哈希资源缓存。

### 闭环记录

- **解决日期**: -
- **解决方案**: -
- **验证方式**: -
- **证据**: -

---

### [2026-07-15] All OFF后GP24仍保持3.3V

- **状态**: in_progress
- **步骤**: S22-E
- **发现时间**: 2026-07-15
- **相关HVR**: `.emv2/checkpoints/HVR-S22-001.md`
- **描述**: 用户关闭RGB灯光后实测GP24仍为3.3V，未达到硬门控预期。

### 分析

- 已确认 `build/GP2040-CE.elf` 和Fightpad12Slim UF2生成于16:22，而门控源码修改于17:57至18:04。
- 两个相关OBJ仍为16:21；现有ELF符号表中不存在 `setBoostPower` 和 `g_menuRgbPowerEnabled`。
- 当前实测使用的build产物不包含GP24门控实现，因此GP24仍保持旧固件的常高行为。
- 源码搜索未发现其他GP24写入者；菜单addon先于灯光addon执行，当前无需追加代码修复。

### 解决方案

- 用户重新构建，使 `build/GP2040-CE_0.0.0_Fightpad12Slim.uf2` 时间戳晚于门控源码。
- 烧录新UF2后使用RGB Customize的`All OFF`复测GP24；预期在约20ms加黑帧/1ms关断延时后变为0V。
- 若新产物仍失败，再增加运行时GPIO回读诊断，不在旧产物结果上继续修改状态机。

### 闭环记录

- **解决日期**: -
- **解决方案**: -
- **验证方式**: -
- **证据**: -

---

## 历史归档

<!-- 已闭环问题归档索引 -->

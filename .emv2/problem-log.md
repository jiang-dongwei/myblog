# 问题追踪

## 当前问题

### [2026-08-18] Web Config Button预览按B1退出时卡死

- **状态**: retest_pending
- **步骤**: S50-D
- **发现时间**: 2026-08-18
- **相关HVR**: -
- **描述**: Web Config初始页按B1可进入Button预览，其他按键预览正常，但再次按B1退出时程序卡住。

### 分析

- UF2生成时间16:19晚于相关源码16:15，已排除未重新编译或烧录旧源码。
- B1退出会销毁`ButtonLayoutScreen`，其`shutdown()`连续注销Profile Change和USB Host事件回调。
- `EventManager::unregisterEventHandler()`的回调遍历循环错误使用`it++`，没有推进`funcIt`；目标不是首项时会重复检查同一回调并破坏外层迭代器，形成退出卡死。

### 解决方案

- 将错误的循环增量改为`funcIt++`。
- B1继续在按下边沿请求返回，并在旧Button页绘制前切到`CONFIG_INSTRUCTION`，所以不显示B1动画。
- `ConfigScreen`在进入时若仍有按钮按住，只等待这次释放，不把同一次B1松开当作重新进入命令。

### 闭环记录

- **解决日期**: -
- **解决方案**: 源码根因已修复，等待用户重新构建烧录复测
- **验证方式**: B1进入、B1退出、连续重复10次、其他按键动画及正常游戏模式B1
- **证据**: `src/eventmanager.cpp`、`src/display/ui/screens/ButtonLayoutScreen.cpp`

---

### [2026-08-13] PS5 USB环境下BLE Profile只改RAM未可靠落盘

- **状态**: retest_pending
- **步骤**: S43-D
- **发现时间**: 2026-08-13
- **相关HVR**: -
- **描述**: PS蓝牙切换到Xbox后当次可配对，但Xbox配对成功再关机重启会重新进入配对并最终回到PS。

### 分析

- Bluetooth Type菜单原来触发`GPStorageSaveEvent(false)`；`Storage::save(false)`在USB Host启用、PS4/PS5驱动且需要认证时直接拒绝保存。
- 菜单已经同时写入`has_bluetoothProfile=true`和目标值，但失败结果未被使用，Proxy仍可能把仅存在RAM的新Profile发送给C6。
- 重启后RP2350从Flash读回旧PS并发给已保存Xbox的C6，C6将其视为真实Profile变化，因此清理不兼容bond并重新配对。

### 解决方案

- BLE Profile菜单改用`GPStorageSaveEvent(true, false)`，仅对此明确配置动作强制落盘且不重启RP2350。
- 增加保存进行中门控，Proxy在Flash结果未知时不发送目标Mode；成功后下一轮同步，失败则恢复旧`has_`和值并显示`Save Failed`。
- 启动配置读取检查`has_bluetoothProfile`，缺失时使用独立Xbox默认值；不读取USB`inputMode`。
- 普通Mode帧继续只使用`APPLY_NOW=0x01`，不发送`FORCE_REPAIR`。

### 闭环记录

- **解决日期**: -
- **解决方案**: 源码与静态验证完成，等待用户构建烧录及四场景实测
- **验证方式**: PS->Xbox、Xbox普通重启、30秒超时、USB PS5/BLE Xbox独立性
- **证据**: `docs/ESP32C6_BLE_PROFILE_HANDOFF.md`

---

### [2026-08-12] 多 BLE Profile 改造后 GPIO13 配对无 Pairing

- **状态**: handoff_ready
- **步骤**: S41-F / S41-G
- **发现时间**: 2026-08-12
- **相关HVR**: -
- **描述**: ESP32-C6 增加多 Profile、绑定准入和定向重连后，实体 BT 挡按下 GPIO13 配对键，OLED 没有稳定进入 Pairing；此前单 Profile 固件的按键响应正常。

### 分析

- GPIO13 直接属于 ESP32-C6，RP2350 已具备 `FS status=0x03` 解析、Pairing OLED 页面和灯效，因此问题主路径不在 RP2350。
- C6 当前 `update_ble_status_output()` 先判断 `hid_connected`，后判断 `pairing_status_active()`；已连接时显式 Pairing 会被 Connected 遮住。
- `trigger_pairing_mode()` 打开窗口后只异步请求断链，应立即发送 Pairing 状态，并确保断链期间 Pairing 优先级不被覆盖。
- C6 使用启动瞬间采样的 `s_pair_button_idle_level` 判断按下方向；若日志没有 `pair button debounced: pressed`，需按实测电平改为明确有效电平。
- `BLE_PROFILE_FLAG_FORCE_REPAIR` 当前在 C6 仅记录日志、没有动作；它不是 GPIO13 的直接原因，但属于协议未完成项。

### 解决方案

- 已新增 `docs/ESP32C6_GPIO13_PAIRING_REGRESSION_HANDOFF.md`，要求 C6 端将 Pairing 提升到 Connected 之上、按键触发后立即发送 `FS 03`、可靠断开旧链路并保持30秒快速普通广播。
- 只有按键边沿日志缺失时才修改 GPIO13 电平判定，并以实测“释放/按下”电平为依据。
- RP2350 本轮不增加无关源码补丁；等待 ESP32-C6 AI 修改、构建和双端实机验证。

### 闭环记录

- **解决日期**: -
- **解决方案**: C6修复任务书已完成，源码与实机修复待执行
- **验证方式**: GPIO13按键日志、`FS 03`、OLED Pairing、断链及30秒广播完整链路
- **证据**: `docs/ESP32C6_GPIO13_PAIRING_REGRESSION_HANDOFF.md`

---

### [2026-08-10] USB PS3模式能枚举但无按键输入

- **状态**: in_progress
- **步骤**: S39-D
- **发现时间**: 2026-08-10
- **相关HVR**: `.emv2/checkpoints/HVR-S39-001.md`
- **描述**: 最新UF2烧录后，PC能够识别PS3控制器，但测试平台没有按键输入；蓝牙模式及其他USB模式输入正常。

### 分析

- 构建时间戳确认PS3源码修改已进入对象文件、ELF和UF2，排除旧产物。
- 蓝牙输入正常，排除物理按键、Profile映射和通用输入扫描故障。
- Switch、PS4、PS5和Xbox在同一USB硬件档位正常，问题集中于PS3专用USB描述符、HID报告提交或PC主机侧DS3接收链路。
- S39设备子类型归一未解决故障，说明残留Arcade类型不是唯一根因。
- Windows枚举属性确认标准微软HidUsb驱动正常；绕过网页直接读取HID接口，在20秒持续按键期间仍收到0个输入报告，排除测试网站原因。
- PS3 HID描述符声明的输入长度为49字节，但原代码以`sizeof(PS3Report)`提交51字节，主机按描述符长度丢弃报告与“枚举正常、输入为0”现象一致。

### 解决方案

- 增加`PS3_INPUT_REPORT_SIZE=49`，中断IN发送与控制`GET_REPORT`均只向主机返回49字节；保留现有结构、按键映射和其他USB模式。
- 等待用户重新构建烧录后，以网页显示和Windows原始HID 49字节报告共同复测。

### 闭环记录

- **解决日期**: -
- **解决方案**: -
- **验证方式**: PS3输入测试及其他USB模式回归
- **证据**: `.emv2/checkpoints/HVR-S39-001.md`

---

### [2026-08-10] GP33 USB/BT实体挡位与初始软件极性相反

- **状态**: retest_pending
- **步骤**: S37-D
- **发现时间**: 2026-08-10
- **相关HVR**: -
- **描述**: 拔掉USB后，实体BT挡蓝牙消失；实体USB挡可以扫描到蓝牙但无法连接。

### 分析

- 该组合现象与“实体USB挡实际读低、实体BT挡实际读高”完全一致。
- 初始判断把原理图触点旁的USB/BT文字直接当作装配后拨杆方向，但滑动开关机械方向可能与触点位置相反。
- USB挡仍能扫描到蓝牙，说明初始固件在该挡位错误地使能了GP34，同时传输状态与用户选择不一致，因此可见但不可正常使用。

### 解决方案

- Fightpad12Slim板级极性改为 `USB_LEVEL=0`、`BT_LEVEL=1`。
- GP34目标语义保持不变：USB挡拉低，BT挡拉高；30ms运行时消抖保持不变。
- 重新构建烧录后，验证BT挡可广播连接、USB挡蓝牙消失，并测量GP33/GP34电平作为最终证据。

### 闭环记录

- **解决日期**: -
- **解决方案**: 已完成源码返工，等待实机复测
- **验证方式**: BT/USB挡蓝牙行为及GP33/GP34电压
- **证据**: -

---

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

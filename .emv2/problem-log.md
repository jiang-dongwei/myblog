# 问题追踪

## 当前问题

### [2026-08-13] Xbox Profile不可发现且Web Gamepad映射为unknown

- **状态**: in_progress
- **步骤**: S5-D / S5-E / S5-F
- **发现时间**: 2026-08-13
- **相关HVR**: `.emv2/checkpoints/HVR-S5-001.md`
- **描述**: Xbox模式蓝牙搜不到；PS5模式只体现名称变化；测试网站显示
  `Unknown Gamepad (0000 0000)`及`mapping: unknown`。

### 分析

- Xbox Complete Name使Legacy Advertising内容超过31字节，导致广播字段设置失败。
- 蓝牙名称不会让Web Gamepad API成为Xbox；Chromium Windows依赖设备VID/PID
  选择标准映射器，并按特定Button/Axis Usage索引读取Xbox Bluetooth报告。
- 当前Xbox Profile使用项目VID/PID及紧凑11按钮布局，无法触发浏览器的
  `MapperXboxBluetooth`。

### 解决方案

1. 所有Profile恢复`FP12Slim-C6`名称，避免名称伪装和广播溢出。
2. 仅Xbox Profile调整为Chromium识别的Xbox One S Bluetooth身份和HID布局。
3. 按浏览器映射器要求布置Button 1..15和X/Y/Z/Rx/Ry/Rz/Hat，继续不提供振动。
4. 增加描述符、报告字节和广播长度静态测试，再编译ESP32-C6。

### 闭环记录

- **解决日期**: -
- **解决方案**: -
- **验证方式**: Windows蓝牙重新配对和相同测试网站
- **证据**: 用户截图及后续实机结果

### 第二轮实机结果

- Xbox模式已经可以搜索和连接，广播长度问题已解决。
- 新问题1：断开后没有自动重连。
- 新问题2：菜单选择Xbox蓝牙手柄后，主页面仍显示PS5。
- 待确认：C6是否收到Mode、是否ACK并重启；RP2350是否消费ACK并更新持久化/显示状态。

### 第二轮修复

- 已确认RP2350主页面`PS5`显示的是有线USB InputMode，不是Bluetooth Profile；
  Bluetooth Type选择本来就不应改写该标签，也不需要重启RP2350。
- C6运行时收到不同Profile会返回Restarting ACK并自行重启；启动同步窗口内收到Profile
  则在本次启动直接应用。
- 断线重连改为GAP单一状态权威；断线时补查bond store；定向快速广播后回退到
  白名单过滤的普通广播，仅原绑定设备可连接。
- 地址下发前把identity类型规范化为控制器接受的PUBLIC/RANDOM类型。
- 静态检查、4组宿主测试和ESP32-C6完整编译均通过；等待第三轮实机验证自动重连。

### 第三轮实机结果

- PS5 Profile可以连接；Xbox 1708 Profile按GPIO13后PC搜不到设备。
- Xbox Profile切换后设备重新启动，但用户观察到开机仍回到PS5。

### 第三轮根因与修复

- 1708描述符有4个Report特征，NimBLE配置上限只有3，导致Xbox HID服务启动失败；上限已
  改为4，并增加编译期保护和启动日志`max_rpts=4`。
- v5迁移遗漏了存储加载器中的v4入口；改为统一的v1~v5迁移判断，避免以后只更新定义
  却漏更新加载器。
- 测试默认改成Xbox：全新/无效NVS及v1~v5升级均一次性强制Xbox、清bond、开30秒配对。
- 状态：源码、4组宿主测试、ESP32-C6编译已通过；等待第四轮实机广播/配对验证。

### 第四轮实机结果与修复

- 实测开机会清配对、Xbox在网站仍为Unknown、重启后Profile又回PS5。
- C6根因：v6默认/迁移人为附加了清bond和自动配对pending；现已改为普通启动与升级都
  保留bond，旧版迁移同时保留原Profile。
- RP2350根因：蓝牙菜单只写`bluetoothProfile`数值，没有置
  `has_bluetoothProfile=true`，序列化后配置可能被当成未设置；现已补齐有效位，并为旧
  配置初始化独立的BLE Profile默认值。USB `inputMode`与BLE `bluetoothProfile`继续分开。
- 网站Unknown并非蓝牙名称问题。Windows没有把该标准BLE HOGP设备暴露为真正Xbox
  XInput/WGI身份时，浏览器只会得到Generic/Unknown设备；复制PnP ID和报告描述符不足以
  保证Xbox标签。
- C6三组宿主测试及完整编译通过；RP2350只做静态检查，等待用户自行编译烧录并实测。

---

## 历史归档

# ESP32-C6 BLE HID Gamepad — 蓝牙架构文档

> 本文档以 `main/main.c` 为核心，结合 NimBLE 源码，全面解释本项目蓝牙子系统的工作原理。

---

## 目录

1. [架构总览](#1-架构总览)
2. [BLE 地址体系](#2-ble-地址体系)
3. [安全与配对 (Bonding)](#3-安全与配对-bonding)
4. [NVS 持久化存储](#4-nvs-持久化存储)
5. [启动流程](#5-启动流程)
6. [广播流程](#6-广播流程)
7. [连接流程](#7-连接流程)
8. [断连与重连](#8-断连与重连)
9. [配对窗口机制](#9-配对窗口机制)
10. [代码中的 BLE 状态变量](#10-代码中的-ble-状态变量)
11. [NimBLE 启动时自动完成的 IRK 恢复](#11-nimble-启动时自动完成的-irk-恢复)
12. [双设备重启无法重连 — 问题分析](#12-双设备重启无法重连--问题分析)

---

## 1. 架构总览

```
┌──────────────┐     UART (GPIO16/17)     ┌──────────────┐
│   RP2350     │ ◄──────────────────────► │   ESP32-C6   │
│  (主控芯片)   │   8字节帧协议              │  (BLE HID)   │
└──────────────┘                          └──────┬───────┘
                                                 │ BLE
                                                 ▼
                                         ┌──────────────┐
                                         │  手机 / 主机  │
                                         │ (Android/iOS │
                                         │  /Windows)   │
                                         └──────────────┘
```

**ESP32-C6 的角色**：BLE Peripheral（从设备），作为 HID Gamepad。

**数据流方向**：
- RP2350 → ESP32-C6：手柄按键/摇杆数据（UART 二进制帧）
- ESP32-C6 → 手机：HID 游戏手柄报告（BLE GATT Notify）
- ESP32-C6 → RP2350：固件信息、BLE 连接状态（UART 二进制帧）

---

## 2. BLE 地址体系

### 2.1 地址类型总览

ESP32-C6 **没有 Public Address**（公共地址需要向 IEEE 购买，芯片没有预烧），所以全部靠随机地址。

| 地址类型 | 英文名 | 特征 | 本项目是否使用 |
|----------|--------|------|---------------|
| 公共地址 | Public Address | IEEE 分配，全球唯一，不变 | ❌ ESP32-C6 无 |
| 静态随机地址 | Static Random Address | 上电时生成/读取，**不变** | ✅ **当前正在用** |
| 不可解析私有地址 | NRPA | 随机生成，定期变化，不可追踪 | ❌ |
| 可解析私有地址 | RPA | 用 IRK 生成，配对设备可解析 | ❌ `privacy=0` 禁用了 |

### 2.2 本项目的地址选择逻辑

代码位置：[main.c:879-910](main/main.c#L879-L910) `ensure_ble_address()`

```c
// 第1步：确保有一个随机地址（从控制器读取或新生成）
ble_hs_util_ensure_addr(0);              // prefer_random = 0 = false
//   ↓ NimBLE 内部会：
//   1. 尝试读 BLE_ADDR_PUBLIC → 没有
//   2. 调 ble_hs_util_ensure_rand_addr()
//      → 从控制器读取静态地址 (esp_ble_hw_get_static_addr)
//      → 读不到就随机生成一个新地址 (ble_hs_id_gen_rnd)
//      → 设置到控制器 (ble_hs_id_set_rnd)

// 第2步：确定广播时用哪种地址类型
ble_hs_id_infer_auto(0, &s_own_addr_type);  // privacy = 0 = 不用 RPA
//   ↓ privacy=0 时的优先级：
//   1. BLE_OWN_ADDR_RANDOM  ← 因为上面设置了随机地址，所以选这个
//   2. BLE_OWN_ADDR_PUBLIC  ← 没有，跳过
```

**结论**：当前代码用 **BLE_OWN_ADDR_RANDOM**（静态随机地址）进行广播。

### 2.3 关键问题：静态地址是否跨重启保持不变？

看 `ble_hs_util_ensure_rand_addr()` 的逻辑（`addr.c:56-84`）：

```
1. ble_hs_id_copy_addr(BLE_ADDR_RANDOM, ...)  ← 检查 NimBLE 内部是否已记录地址
   ├── 有 → 直接用（同一个 session 内，因为 ble_hs_id_rnd 存在内存里）
   └── 没有（重启后）
2. ble_hs_util_load_rand_addr()  ← 从控制器硬件读取
   └── esp_ble_hw_get_static_addr()  ← ESP32-C6 控制器提供的 API
       ├── 成功 → 用硬件提供的地址（跨重启不变 ✅）
       └── 失败
3. ble_hs_id_gen_rnd(0, ...)  ← 随机生成新地址（跨重启会变 ❌）
```

**ESP32-C6 的 `esp_ble_hw_get_static_addr()` 行为**：根据 ESP-IDF 文档，这个 API 读取的是芯片 eFuse 中烧录的地址或控制器生成的地址。**正常情况下，每次重启返回相同的地址**。

但是，如果在某些条件下这个 API 失败了，NimBLE 就会生成一个新地址 → 手机不认识 → 无法自动重连。

### 2.4 RPA 与隐私模式

RPA (Resolvable Private Address) 的生成公式：

```
RPA = hash(IRK, prand)
```

- `IRK`：配对时双方交换的 16 字节密钥，存储在 NVS 中
- `prand`：随机数（RPA 地址的低 3 字节）
- `hash`：AES-128 加密函数

**手机如何识别 RPA 广播的外设**：
1. 手机收到 RPA 地址
2. 手机用存储的所有 IRK 逐个尝试解析
3. 哪个 IRK 能解出匹配的地址 → 就找到了对应的设备
4. 自动发起连接

---

## 3. 安全与配对 (Bonding)

代码位置：[main.c:1027-1036](main/main.c#L1027-L1036) `configure_security()`

```c
ble_hs_cfg.sm_bonding   = 1;   // 启用 Bonding（配对信息持久化）
ble_hs_cfg.sm_sc        = 1;   // 启用 LE Secure Connections（安全级别更高）
ble_hs_cfg.sm_mitm      = 0;   // 不需要 MITM 保护（NO_IO 场景）
ble_hs_cfg.sm_io_cap    = BLE_SM_IO_CAP_NO_IO;  // 无输入无输出能力
ble_hs_cfg.sm_our_key_dist   = ENC | ID;  // 我们分发的密钥：加密密钥 + 身份密钥(IRK)
ble_hs_cfg.sm_their_key_dist = ENC | ID;  // 对方分发的密钥：加密密钥 + 身份密钥(IRK)
```

### 3.1 配对过程（简化）

```
ESP32                           手机
  │                               │
  │──── 广播 (ADV_IND) ────────►  │
  │                               │
  │◄─── 连接请求 ──────────────   │
  │                               │
  │◄─── 配对请求 (Pairing Req) ── │  ← 手机发起加密
  │                               │
  │──── 配对响应 ───────────────►  │
  │                               │
  │◄═══ LE Secure Connections ═══►│  ← 密钥协商（ECDH 椭圆曲线）
  │                               │
  │◄═══ 密钥分发 ═══════════════► │  ← 交换 LTK, IRK, CSRK
  │                               │
  │       配对完成，Bond 已存储      │
```

### 3.2 配对后手机存储了什么

| 密钥 | 缩写 | 用途 |
|------|------|------|
| Long Term Key | LTK | 加密后续连接 |
| Identity Resolving Key | IRK | 解析 RPA 地址，识别设备 |
| Connection Signature Resolving Key | CSRK | 数据签名 |

### 3.3 配对后 ESP32 存储了什么

ESP32 把配对信息存两份（NimBLE 设计）：

| 存储对象 | 内容 | 含义 |
|----------|------|------|
| `our_sec` | 我们的 LTK、IRK、CSRK | 我们自己的密钥 |
| `peer_sec` | 对方的 LTK、IRK、CSRK、地址 | 手机的密钥和地址 |

---

## 4. NVS 持久化存储

### 4.1 存储位置

NVS 分区，命名空间 `nimble_bond`。

### 4.2 存储条目

```
NVS (nimble_bond)
├── our_sec_1        ← 我们的配对密钥 (struct ble_store_value_sec, 60+ bytes)
├── peer_sec_1       ← 手机的配对密钥和地址
├── cccd_sec_1       ← GATT 特征描述符订阅状态
├── local_irk_1      ← 本地 IRK（注意：独立于 our_sec 存储！）
└── rpa_rec_1        ← RPA 地址映射记录
```

### 4.3 持久化时机

配对完成后，NimBLE 自动调用 `ble_store_config_write()` → `ble_store_nvs_write()` → `nvs_set_blob()`，**完全自动，不需要手动调用**。

### 4.4 恢复时机

`ble_store_config_init()` (main.c:1131) → `ble_store_config_conf_init()` (内部) → `ble_nvs_restore_sec_keys()`：
- 从 NVS 读取所有 `our_sec_*`、`peer_sec_*`、`cccd_sec_*` 等
- 恢复到 RAM 中的 `ble_store_config_our_secs[]`、`ble_store_config_peer_secs[]` 等数组

---

## 5. 启动流程

`app_main()` 的 BLE 相关部分：

```
1. nvs_flash_init()                ← NVS 初始化（存储配对信息的底层）
2. init_nimble_controller()        ← 蓝牙控制器初始化
   ├── esp_bt_controller_mem_release(CLASSIC_BT)  ← 释放经典蓝牙内存
   ├── esp_bt_controller_init()    ← 初始化 BLE 控制器
   ├── esp_bt_controller_enable(BLE)
   └── esp_nimble_init()           ← 初始化 NimBLE 协议栈
3. configure_security()            ← 配置安全参数（bonding, SC, key dist）
4. esp_hidd_dev_init()             ← 初始化 HID 设备
   └── 触发 ESP_HIDD_START_EVENT  ← 回调 hidd_event_callback()
5. ble_store_config_init()         ← 从 NVS 恢复配对信息
   └── 内部调用 ble_nvs_restore_sec_keys()
       → 恢复 our_sec, peer_sec, cccd, local_irk, rpa_rec
6. esp_nimble_enable(host_task)    ← 启动 NimBLE Host 任务
   └── ble_hs_sync() → ble_hs_startup_go()
       ├── ble_hs_pvcy_set_default_irk()   ← 从 NVS 加载 IRK（没有就生成）
       ├── ble_hs_pvcy_set_our_irk(NULL)   ← 设置 IRK 到隐私模块
       └── ble_hs_misc_restore_irks()      ← 把已配对设备的 IRK 加到解析列表
7. open_pairing_window()           ← 上电默认打开配对窗口 60 秒
```

---

## 6. 广播流程

### 6.1 广播参数

| 参数 | 值 | 代码位置 |
|------|-----|---------|
| 设备名 | `"FP12Slim-C6"` | 定义第 34 行 |
| 广播间隔 | 100ms ~ 200ms | 第 956-957 行 |
| 连接模式 | Undirected | 第 954 行 |
| 发现模式 | General Discoverable | 第 955 行 |
| 广播时长 | `BLE_HS_FOREVER`（永不超时） | 第 959 行 |
| 广播内容 | Flags + 设备名 + HID UUID (0x1812) + Appearance (Gamepad) | 第 936-945 行 |

### 6.2 广播的 5 个触发时机

| 触发条件 | 代码位置 |
|----------|---------|
| HID 服务启动完成 (`ESP_HIDD_START_EVENT`) | 第 985 行 |
| BLE 连接失败 (`BLE_GAP_EVENT_CONNECT` status != 0) | 第 833 行 |
| 断连 (`BLE_GAP_EVENT_DISCONNECT`) | 第 841 行 |
| 广播完成 (`BLE_GAP_EVENT_ADV_COMPLETE`) | 第 850 行 |
| USB 切换到蓝牙模式 (`set_transport_bt_enabled(true)`) | 第 358 行 |
| `pair_button_task` 保活（每 10ms 检查） | 第 460-465 行 |

### 6.3 广播的 5 个拦截条件

`start_advertising()` 会在以下情况直接 return（不广播）：

```c
if (!s_hid_started || !transport_bt_enabled())   return;  // HID 未就绪 或 USB 模式
if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE)     return;  // 已经连接
if (ble_gap_adv_active())                          return;  // 已经正在广播
if (!s_addr_ready && !ensure_ble_address())        return;  // 地址还没就绪
```

---

## 7. 连接流程

### 7.1 事件处理

BLE 连接涉及两个事件通道：

| 通道 | 来源 | 事件 |
|------|------|------|
| `gap_event()` | NimBLE GAP 层 | CONNECT, DISCONNECT, ENC_CHANGE, SUBSCRIBE, ADV_COMPLETE, REPEAT_PAIRING |
| `hidd_event_callback()` | ESP-HID 层 | START, CONNECT, DISCONNECT, PROTOCOL_MODE, CONTROL, OUTPUT, STOP |

### 7.2 连接成功时的执行顺序

```
手机发起连接
  ↓
BLE_GAP_EVENT_CONNECT (status=0)
  ├── s_conn_handle = conn_handle
  ├── close_pairing_window()           ← 连上了就不需要配对窗口了
  ├── send_ble_status_frame(CONNECTING) ← 通知 RP2350 "连接中"
  └── (不调 start_advertising，因为连接成功后广播自动停止)
  ↓
BLE_GAP_EVENT_ENC_CHANGE                ← 加密层建立
  ↓
BLE_GAP_EVENT_SUBSCRIBE                 ← 手机订阅了 HID 特征值通知
  ├── ensure_report_task_running()      ← 启动手柄报告任务
  └── send_ble_status_frame(CONNECTED)  ← 通知 RP2350 "已连接"
  ↓
手柄正常工作
```

---

## 8. 断连与重连

### 8.1 断连处理

```
手机断连（或超出距离）
  ↓
BLE_GAP_EVENT_DISCONNECT
  ├── s_conn_handle = NONE
  ├── send_ble_status_frame(DISCONNECTED) ← 通知 RP2350 "未连接"
  └── start_advertising()                ← 立即开始广播，等待重连
  ↓
ESP_HIDD_DISCONNECT_EVENT
  ├── s_conn_handle = NONE
  ├── send_ble_status_frame(DISCONNECTED)
  └── start_advertising()
```

### 8.2 重连流程（当前 `privacy=0` 的行为）

**场景 A：只重启手机（ESP32 没断过电）**
```
手机重启 → 手机扫描 BLE
  → 看到 ESP32 的广播（静态地址）
  → 手机 NVS 中有这个静态地址的配对记录
  → 自动连接 ✅
```

**场景 B：只重启 ESP32（手机没断过电）**
```
ESP32 重启 → 上电广播
  → 手机蓝牙栈缓存中还有这个设备
  → 手机看到广播 → 自动连接 ✅
```

**场景 C：两个都重启**
```
手机重启 + ESP32 重启
  → ESP32 广播（静态地址）
  → 手机扫描，看到 ESP32
  → 但手机不认为这是"已配对设备"！！
  → 可以在"可用设备"列表中看到，但需要手动点击连接
  → 手动点击后 → 触发 REPEAT_PAIRING → 删除旧 bond → 重新配对
  → 能连上 ✅，但需要手动操作
  → 原因：见第 12 节
```

---

## 9. 配对窗口机制

### 9.1 设计目的

配对窗口（Pairing Window）是一个 60 秒的时间窗口，在这期间：
- ESP32 允许新设备配对
- ESP32 广播自己（可被发现）

配对窗口过期后：
- 配对窗口状态变为 false
- 但在 S3 修改后，**广播不会停止**（广播已与配对窗口解耦）

### 9.2 配对窗口状态机

```
                         GPIO13 按下
     ┌──────────────────────────────────────┐
     │                                      │
     ▼                                      │
 [窗口关闭] ── GPIO13 按下 ──► [窗口打开]    │
     ▲                          │           │
     │                          │ 60 秒超时  │
     └──────────────────────────┘           │
     │                                      │
     └── 连接成功 (close_pairing_window) ───┘
```

### 9.3 GPIO13 配对按键流程

`trigger_pairing_mode()` 第 384 行：
1. 如果 USB 模式 → 忽略按键
2. 如果窗口已打开 → 确保广播在运行
3. 如果窗口关闭 → 停止当前广播 + 断开当前连接 + **清除所有 bond** + 重新广播

**注意**：`ble_store_clear()` 会删除 NVS 中所有配对信息！

---

## 10. 代码中的 BLE 状态变量

| 变量 | 类型 | 初始值 | 含义 |
|------|------|--------|------|
| `s_hid_started` | bool | false | HID 服务是否已启动 |
| `s_conn_handle` | uint16_t | `BLE_HS_CONN_HANDLE_NONE` (0xFFFF) | 当前 BLE 连接句柄 |
| `s_pairing_window_open` | bool | false（上电后 app_main 设为 true） | 配对窗口是否打开 |
| `s_transport_bt_enabled` | bool | false（app_main 设为 true） | 蓝牙模式/USB 模式 |
| `s_own_addr_type` | uint8_t | 0 | 广播地址类型（RANDOM/PUBLIC/RPA） |
| `s_addr_ready` | bool | false | 地址是否已就绪 |
| `s_ble_status` | uint8_t | 0xFF | BLE 状态（发给 RP2350） |

---

## 11. NimBLE 启动时自动完成的 IRK 恢复

这是 NimBLE 内部自动完成的，完全不需要手动干预：

### 11.1 步骤 1：加载本地 IRK

`ble_hs_pvcy_set_default_irk()` (`ble_hs_pvcy.c:203`)：

```
1. 读 NVS 中的 local_irk_1
   ├── 找到了 → 复制到 ble_hs_pvcy_default_irk[16]
   └── 没找到
       2. 随机生成 16 字节 IRK
       3. 写入 NVS local_irk_1（下次重启就能读到了）
```

### 11.2 步骤 2：把 IRK 加载到控制器

`ble_hs_pvcy_set_our_irk(NULL)` (`ble_hs_pvcy.c:253`)：

```
1. 把 default_irk 复制到 ble_hs_pvcy_irk（当前活跃的 IRK）
2. 将零地址 + IRK 加入控制器的解析列表
   （这个"零地址条目"用于为非定向广播生成 RPA）
```

**注意**：只有在 `privacy=1` 时这个 IRK 才会被用来生成 RPA。`privacy=0` 时，这些步骤仍然执行，但 RPA 不被使用。

### 11.3 步骤 3：恢复已配对设备的 IRK 到解析列表

`ble_hs_misc_restore_irks()` (`ble_hs_misc.c:141`)：

```
遍历 NVS 中所有 peer_sec_* 记录：
  如果某条记录有 irk_present：
    调用 ble_hs_pvcy_add_entry(peer_addr, peer_addr_type, peer_irk)
    → 把该设备的 IRK 加入控制器的解析列表
```

**这个函数的意义**：当手机用 RPA 地址连接 ESP32 时，ESP32 的控制器能够解析手机的 RPA，从而识别出"这是之前配对过的手机"。

---

## 12. 双设备重启无法重连 — 问题分析

### 12.1 问题现象

| 场景 | 结果 |
|------|------|
| 只重启手机 | ✅ 自动重连 |
| 只重启 ESP32 | ✅ 自动重连 |
| 两个都重启 | ❌ 手机可扫描到但不能自动连接，需手动点击 |

### 12.2 已确认的事实

| 项目 | 结论 |
|------|------|
| BLE 地址跨重启是否一致？ | ✅ **一致**（已通过日志验证） |
| 手动点击能否连接？ | ✅ **能**（说明 bond 数据完好，LTK 加密能正常工作） |
| Bond 数据是否持久化？ | ✅ **是**（NimBLE 自动管理 NVS 存储） |
| 手机能否扫描到？ | ✅ **能**（说明广播本身没问题） |

### 12.3 排除的根因

- ❌ ~~BLE 地址在重启后变了~~ — 已验证地址一致
- ❌ ~~Bond 数据损坏~~ — 手动点击能连上，说明 LTK 加密正常

### 12.4 最可能的根因：时序竞争（Timing Race）

**核心思想**：两台设备同时冷启动时，ESP32 的初始化时间（约 1-2 秒）比手机蓝牙栈的初始化时间更长。手机的自动重连扫描窗口可能在 ESP32 开始广播**之前**就已经完成了。

```
时间线（两个都重启）：

手机:  [开机]──[蓝牙栈启动]──[加载bond]──[扫描已配对设备]──[超时放弃]──[空闲]
                                          ↑ 扫描窗口可能只有几秒
ESP32: [开机]──[bootloader]──[NVS初始化]──[控制器初始化]──[HID启动]──[开始广播]
                                                              ↑ 1-2秒后才开始广播
                                                              
→ 手机扫描时，ESP32 还没开始广播
→ 手机没找到 FP12Slim-C6
→ 手机放弃本次自动重连
→ 用户手动打开蓝牙设置 → 触发主动扫描 → 看到 ESP32 → 点击连接 ✅
```

**为什么"只重启手机"可以？**
```
ESP32 一直在广播（已经广播了很长时间）
手机重启 → 蓝牙启动 → 扫描 → 立刻看到 ESP32 → 自动连接 ✅
```

**为什么"只重启 ESP32"可以？**
```
手机蓝牙一直在运行，持续后台扫描
ESP32 重启 → 开始广播 → 手机后台扫描捕获到 → 自动连接 ✅
```

### 12.5 加剧因素：广播间隔太慢

在 S2 功耗优化中，广播间隔从 **30-50ms 改为 100-200ms**：

```c
// main.c:956-957 (当前值)
adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(100);  // S2 改之前是 30
adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(200);  // S2 改之前是 50
```

广播间隔越慢 → 手机后台扫描捕获到的概率越低 → 自动重连越不可靠。

### 12.6 验证方法

**测试 1：错开启动时序**
1. 先给 ESP32 上电，等待 **10 秒**（确保广播已开始）
2. 再启动手机
3. 观察是否能自动重连

如果这样能自动重连 → **确认是时序竞争问题**。

**测试 2：恢复快速广播间隔**
1. 把广播间隔临时改回 30-50ms
2. 两个设备同时冷启动
3. 观察是否能自动重连

如果这样能自动重连 → 确认是广播间隔过慢加剧了时序问题。

### 12.7 可能的修复方向

#### 方案 A：启动时先用快广播、连接后降速（推荐）

```c
// 伪代码
if (s_ble_just_booted && !s_ever_connected) {
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(30);   // 启动后前 60 秒快广播
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(50);
} else {
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(100);  // 连过之后降速省电
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(200);
}
```

#### 方案 B：start_advertising() 后立即做一次"快速突发广播"

在 `start_advertising()` 内部，如果刚启动（`s_ever_connected == false`），先用 20ms 间隔广播 5 秒，再切换到 100-200ms。

#### 方案 C：在手机端操作（无法控制）

这个超出 ESP32 的控制范围。Android 的自动重连策略因厂商和版本而异，有些 ROM 对 BLE HID 的自动重连不积极。

---

## 附录：关键 NimBLE API 速查

| API | 功能 | 本项目调用位置 |
|-----|------|--------------|
| `ble_hs_util_ensure_addr(0)` | 确保有 BLE 地址（随机地址优先） | main.c:881 |
| `ble_hs_id_infer_auto(0, &type)` | 推断广播地址类型（privacy=0 不用 RPA） | main.c:887 |
| `ble_gap_adv_start(type, ...)` | 开始 BLE 广播 | main.c:959 |
| `ble_svc_gap_device_name_set()` | 设置 GAP 设备名称 | main.c:1106 |
| `ble_store_config_init()` | 初始化 NVS 存储后端 | main.c:1131 |
| `ble_store_clear()` | 清除所有 bond 信息 | main.c:424 |
| `esp_hidd_dev_init()` | 初始化 HID 设备 | main.c:1123 |
| `esp_hidd_dev_input_set()` | 发送 HID 输入报告 | main.c:473 |

---

## 附录 B：UART 串口协议（ESP32 → RP2350）

> **目标读者**：编写 RP2350 固件的 AI 或开发者。
> **说明**：本章完整描述 ESP32-C6 通过 UART0 (GPIO16/GPIO17, 115200-8N1) 向 RP2350 发送的所有数据帧格式。

### B.1 通用帧格式

所有帧固定 8 字节，统一格式：

```
┌──────────┬──────────┬──────────────────────────────────────┬──────────┐
│ Byte 0   │ Byte 1   │ Byte 2 ~ 6                            │ Byte 7   │
├──────────┼──────────┼──────────────────────────────────────┼──────────┤
│ 0x46     │ type     │ payload (与帧类型相关)                  │ checksum │
│ 固定帧头  │ 帧类型   │ 各类型不同，未使用字节填 0x00           │ XOR(B0~B6)│
└──────────┴──────────┴──────────────────────────────────────┴──────────┘
```

**校验算法**：
```
checksum = B0 ^ B1 ^ B2 ^ B3 ^ B4 ^ B5 ^ B6
```
收到帧后计算校验，如果不匹配则丢弃整帧。

### B.2 帧类型速查

| type (Byte1) | 方向 | 含义 |
|-------------|------|------|
| `0x49` ('I') | ESP32 → RP2350 | 固件信息（仅上电发一次） |
| `0x50` ('P') | RP2350 → ESP32 | 手柄按键/摇杆数据 |
| `0x54` ('T') | RP2350 → ESP32 | 蓝牙/USB 模式切换 |
| `0x42` ('B') | RP2350 → ESP32 | 电池电量 |
| `0x53` ('S') | ESP32 → RP2350 | 蓝牙连接状态 |

### B.3 蓝牙连接状态帧 `0x53` ('S')

**发送时机**：只有在蓝牙状态变化时发送一次，不重复发送。

**帧格式**：

```
Byte 0: 0x46    固定帧头
Byte 1: 0x53    'S' — 蓝牙状态帧
Byte 2: status  状态码 (0x00 / 0x01 / 0x02 / 0x03)
Byte 3: 0x00    (保留)
Byte 4: 0x00    (保留)
Byte 5: 0x00    (保留)
Byte 6: 0x00    (保留)
Byte 7: XOR(B0~B6)
```

**状态码定义**：

| status | 名称 | RP2350 OLED 建议显示 | 触发时机 |
|--------|------|---------------------|---------|
| `0x00` | 未连接 | `蓝牙: 未连接` | 上电初始化完成 / 断连 / 开始广播 |
| `0x01` | 连接中 | `蓝牙: 连接中...` | BLE GAP 连接建立成功，正在加密+初始化 HID |
| `0x02` | 已连接 | `蓝牙: 已连接 ✓` | HID 服务就绪，手柄报告已启动 |
| `0x03` | 配对模式 | `蓝牙: 重新配对...` | GPIO13 配对按键按下，清除了旧配对信息 |

**状态转换图**：

```
             ┌── 上电 ──► 0x00 (未连接)
             │               │
             │               ├── 手机连接成功 ──► 0x01 (连接中) ──► 0x02 (已连接)
             │               │
             │               └── GPIO13 按下 ──► 0x03 (配对模式)
             │                                        │
             │                                        └── 手机连接 ──► 0x01 → 0x02
             │
             │ 已连接时断连 ──► 0x00
             │ 已连接时按 GPIO13 ──► 0x03 (重新配对)
```

**重要说明**：

- `0x01` (连接中) 是在 BLE **连接已经建立**时才发送的，不是在"扫描过程中"发送
- `0x01 → 0x02` 的间隔通常 < 1 秒，RP2350 端显示"连接中..."会一闪而过
- `0x03` 是 GPIO13 按下时立即发送的，不等待手机连接，确保用户立即看到反馈
- ESP32 作为 BLE 从设备，无法感知"手机正在搜索/尝试连接"这个阶段

**各状态校验值参考**：

```
status=0x00: 46 53 00 00 00 00 00 15
status=0x01: 46 53 01 00 00 00 00 14
status=0x02: 46 53 02 00 00 00 00 17
status=0x03: 46 53 03 00 00 00 00 16
```

**RP2350 解析示例（伪代码）**：

```c
#define FRAME_HEADER   0x46
#define FRAME_STATUS   0x53  // 'S'
#define STATUS_DISCONNECTED 0x00
#define STATUS_CONNECTING   0x01
#define STATUS_CONNECTED    0x02
#define STATUS_PAIRING      0x03

void parse_uart_byte(uint8_t byte) {
    static uint8_t frame[8];
    static uint8_t pos = 0;

    // 帧头同步
    if (pos == 0) {
        if (byte == FRAME_HEADER) {
            frame[0] = byte;
            pos = 1;
        }
        return;
    }

    frame[pos++] = byte;
    if (pos < 8) return;
    pos = 0;  // 准备下一帧

    // XOR 校验 (Byte 0~6)
    uint8_t csum = 0;
    for (int i = 0; i < 7; i++) csum ^= frame[i];
    if (frame[7] != csum) return;  // 校验失败，丢弃

    // 根据帧类型分发
    switch (frame[1]) {
    case 0x50:  // 'P' — 手柄报告（RP2350 → ESP32，ESP32 不发送此帧）
        break;

    case 0x53:  // 'S' — 蓝牙状态（仅 ESP32 → RP2350）
        switch (frame[2]) {
        case STATUS_DISCONNECTED:  oled_show("蓝牙: 未连接");     break;
        case STATUS_CONNECTING:    oled_show("蓝牙: 连接中...");  break;
        case STATUS_CONNECTED:     oled_show("蓝牙: 已连接 ✓");   break;
        case STATUS_PAIRING:       oled_show("蓝牙: 重新配对..."); break;
        }
        break;

    case 0x49:  // 'I' — 固件信息（见 B.4 节）
        // 多帧分包协议，见下文
        break;
    }
}
```

### B.4 固件信息帧 `0x49` ('I')

**发送时机**：仅上电时发送一次，在 `C6_DONE` 信号之前。

**内容**：ESP32 的 SDK 版本、平台、主板名、CPU 架构。
```
SDK=5.5.1\nPlat=esp32c6\nBoard=Fightpad12Slim_C6_BLE_HID\nCPU=RISC-V\n
```

**协议**：多帧分包，每帧携带 4 字节 payload。

**帧格式**：

```
Byte 0: 0x46     固定帧头
Byte 1: 0x49     'I' — 固件信息帧
Byte 2: flag+seq 高 2 位 = flag, 低 6 位 = 序号
Byte 3-6: 4 字节 payload 数据
Byte 7: XOR(B0~B6)
```

**flag 定义**：

| flag | 值 | 含义 |
|------|-----|------|
| `0xC0` (bits 7-6 = 11) | 首帧 | |
| `0x80` (bits 7-6 = 10) | 中间帧 | |
| `0x40` (bits 7-6 = 01) | 尾帧 | |
| `0x00` (bits 7-6 = 00) | 单帧（数据 ≤ 4 字节） | |

**RP2350 解析逻辑**：
1. 按帧序号组装 payload
2. 收到尾帧或单帧后，按 `\n` 换行分割字符串
3. 每个字段格式为 `key=value`

### B.5 UART 动作时序（上电）

```
ESP32 上电
  │
  ├── FW_INFO 帧 (0x49)     ← 固件信息，多帧
  │
  ├── "C6_DONE\n" × 20 次   ← 纯文本，不是 8 字节帧！直接接收字符串即可
  │
  ├── 0x00 未连接            ← BLE 状态帧 (0x53)
  │
  └── (之后按状态变化发送 0x53 帧)
```

**注意**：`C6_DONE` 是纯文本（ASCII 字符串 `"C6_DONE\n"`），不是 8 字节二进制帧。RP2350 上电初始化时，先按二进制帧解析，如果收不到合法帧也可能收到 `C6_DONE` 文本，建议两种都处理。

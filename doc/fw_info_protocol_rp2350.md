# ESP32-C6 固件信息帧协议 (RP2350 接收侧说明)

## 1. 概述

ESP32-C6 上电后，在 UART0 初始化完成后，会在发送 `C6_DONE` 信号之前，通过 UART0 TX (GPIO16) 发送自身的固件信息。RP2350 通过 UART RX 接收这些帧，重组后即可获取 ESP32 的 SDK 版本、平台、板卡名称和 CPU 架构。

## 2. 硬件连接

| RP2350 | ESP32-C6 |
|--------|----------|
| UART RX | GPIO16 (UART0 TX) |
| UART TX | GPIO17 (UART0 RX) |
| GND | GND |

- 波特率: **115200 8N1**
- 无硬件流控

## 3. 线路特征（重要）

ESP32-C6 的 UART0 TX 线上同时存在 **两类数据**：

| 类型 | 格式 | 特征 |
|------|------|------|
| Console 日志 | 可打印 ASCII 文本，以 `\n` 结尾 | 不以 `0x46` 开头 |
| 二进制协议帧 | 8 字节固定长度帧 | 以 `0x46` 开头 |

**RP2350 必须用 `0x46` 帧头来过滤**，忽略所有不以 `0x46` 开头的 Console 日志。

## 4. 通用帧格式

所有二进制帧固定 **8 字节**：

```
Byte:  0      1        2           3   4   5   6      7
     [0x46] [Type] [flag/seq]  [  payload ...  ]  [XOR]
```

| 字节 | 含义 |
|------|------|
| 0 | 帧头 `0x46` (ASCII `'F'`) |
| 1 | 帧类型。固件信息帧为 `0x49` (ASCII `'I'`) |
| 2 | flag (bit7-6) + seq (bit5-0)，见下文 |
| 3~6 | 有效载荷数据 |
| 7 | Byte 0~6 的逐字节 XOR 校验值 |

**XOR 校验**：`frame[7] = frame[0] ^ frame[1] ^ ... ^ frame[6]`

校验不通过的帧应丢弃。

## 5. 固件信息帧 (`I` 帧: `0x49`)

### 5.1 Byte2: flag + sequence

```
Bit:   7    6    5    4    3    2    1    0
     [ flag:2bit  ][       seq:6bit         ]
```

| flag (bit7-6) | 值 | 含义 |
|---------------|-----|------|
| `11` | `0xC0` | 首帧 (FIRST) — 多帧序列的第一帧 |
| `10` | `0x80` | 中间帧 (MIDDLE) — 多帧序列的中间帧 |
| `01` | `0x40` | 尾帧 (LAST) — 多帧序列的最后一帧 |
| `00` | `0x00` | 单帧 (SINGLE) — 就只有一帧 |

| seq (bit5-0) | 范围 0~63 |
|--------------|-----------|

### 5.2 Payload 格式 (Byte3~6)

每帧携带 4 字节有效数据。将所有帧按 seq 顺序拼接后得到完整 payload。

payload 是 UTF-8 文本，格式为 `key=value\n`：

```
SDK=5.5.1\n
Plat=esp32c6\n
Board=Fightpad12Slim_C6_BLE_HID\n
CPU=RISC-V\n
```

**字段说明**：

| 字段 | 含义 | 示例值 |
|------|------|--------|
| `SDK` | ESP-IDF 版本号 | `5.5.1` |
| `Plat` | 芯片平台 | `esp32c6` |
| `Board` | 板卡名称 (CMake: `fightpad12slim_c6_ble_hid`) | `Fightpad12Slim_C6_BLE_HID` |
| `CPU` | CPU 指令集架构 | `RISC-V` 或 `Xtensa` |

### 5.3 帧数量

payload 约 60~70 字节，按 4 字节/帧计算约 **15~18 帧**。

以示例 payload (69 字节) 为例：
- seq=0, flag=FIRST : `SDK=`
- seq=1, flag=MIDDLE: `5.5.`
- seq=2, flag=MIDDLE: `1\nPl`
- seq=3, flag=MIDDLE: `at=e`
- ...
- seq=17, flag=LAST (0x51): `\n` (最后的填充可能是 0x00)

## 6. RP2350 接收流程

### 6.1 状态机

```
                   +--------+
    ┌─────────────►│ IDLE   │◄──────────────┐
    │              +--------+               │
    │                  │ byte==0x46         │ (reset if timeout/error)
    │                  ▼                    │
    │              +--------+               │
    │              │ SYNC   │               │
    │              +--------+               │
    │                  │ byte==0x49('I')    │
    │                  ▼                    │
    │              +--------+               │
    │              │ COLLECT│──────────────►│
    │              +--------+  read 6 more  │
    │                  │     bytes          │
    │                  ▼                    │
    │              +--------+               │
    │              │ VERIFY │               │
    │              +--------+               │
    │                  │ XOR ok             │
    │                  ▼                    │
    │              +--------+               │
    └──FIRST/ ─────│ APPEND │────LAST──────►│
       MIDDLE      +--------+               │
                      │                     │
                      ▼ (SINGLE)            │
                  +--------+                │
                  │  PARSE │────────────────┘
                  +--------+
                      │
                      ▼
                  (完成)
```

### 6.2 伪代码

```c
// --- 常量 ---
#define FW_FRAME_MAGIC0    0x46
#define FW_FRAME_TYPE_INFO 0x49
#define FW_FRAME_LEN       8

#define FW_FLAG_SINGLE     0x00
#define FW_FLAG_FIRST      0xC0
#define FW_FLAG_MIDDLE     0x80
#define FW_FLAG_LAST       0x40
#define FW_FLAG_MASK       0xC0
#define FW_SEQ_MASK        0x3F

// --- 接收缓冲区 ---
#define FW_PAYLOAD_MAX     256
static uint8_t  fw_payload[FW_PAYLOAD_MAX];
static uint16_t fw_payload_len = 0;
static bool     fw_receiving = false;

// --- 帧接收函数 (每收到一个 0x46 同步后的 8 字节帧调用一次) ---
bool fw_info_handle_frame(const uint8_t frame[8])
{
    // 1. 校验帧头
    if (frame[0] != FW_FRAME_MAGIC0)  return false;
    if (frame[1] != FW_FRAME_TYPE_INFO) return false;

    // 2. XOR 校验
    uint8_t xor_check = 0;
    for (int i = 0; i < 7; i++) {
        xor_check ^= frame[i];
    }
    if (xor_check != frame[7]) {
        // 校验失败，丢弃
        return false;
    }

    // 3. 解析 flag + seq
    uint8_t flag = frame[2] & FW_FLAG_MASK;
    // uint8_t seq  = frame[2] & FW_SEQ_MASK;  // 可选的序号检查

    // 4. 处理首帧
    if (flag == FW_FLAG_FIRST) {
        fw_payload_len = 0;
        fw_receiving = true;
    }

    if (!fw_receiving && flag != FW_FLAG_SINGLE) {
        return false;  // 非预期的中间帧
    }

    // 5. 追加 payload (byte3~6)
    if (fw_payload_len + 4 > FW_PAYLOAD_MAX) {
        fw_receiving = false;
        return false;  // 溢出
    }

    for (int i = 3; i < 7; i++) {
        fw_payload[fw_payload_len++] = frame[i];
    }

    // 6. 判断是否完成
    if (flag == FW_FLAG_LAST || flag == FW_FLAG_SINGLE) {
        fw_receiving = false;

        // 去掉末尾可能的 0x00 填充
        while (fw_payload_len > 0 && fw_payload[fw_payload_len - 1] == 0x00) {
            fw_payload_len--;
        }

        fw_payload[fw_payload_len] = '\0';  // C 字符串收尾
        return true;  // 接收完成，可调用 fw_info_parse()
    }

    return false;  // 尚未完成
}

// --- 解析 payload 中的 key=value ---
void fw_info_parse(const uint8_t *payload, uint16_t len)
{
    // payload 格式: "SDK=5.5.1\nPlat=esp32c6\nBoard=xxx\nCPU=RISC-V\n"
    // 按 \n 分割，按 = 提取 key/value
    const char *p = (const char *)payload;
    const char *end = p + len;

    while (p < end) {
        // 找 '='
        const char *eq = strchr(p, '=');
        if (!eq) break;
        // 找 '\n'
        const char *nl = strchr(eq + 1, '\n');
        if (!nl) break;

        // 提取 key 和 value
        int key_len = eq - p;
        int val_len = nl - eq - 1;

        if (key_len == 3 && strncmp(p, "SDK", 3) == 0) {
            // sdk_version = 字符串(eq+1, val_len)
        } else if (key_len == 4 && strncmp(p, "Plat", 4) == 0) {
            // platform = 字符串(eq+1, val_len)
        } else if (key_len == 5 && strncmp(p, "Board", 5) == 0) {
            // board = 字符串(eq+1, val_len)
        } else if (key_len == 3 && strncmp(p, "CPU", 3) == 0) {
            // cpu_arch = 字符串(eq+1, val_len)
        }

        p = nl + 1;  // 下一行
    }
}
```

### 6.3 UART 字节流处理

```c
// 逐字节喂入的帧同步器 (与 ESP32 侧 parse_uart_byte 逻辑对称)
static uint8_t  frame_buf[FW_FRAME_LEN];
static uint8_t  frame_pos = 0;

void fw_info_feed_byte(uint8_t byte)
{
    if (frame_pos == 0) {
        // 等待帧头 0x46
        if (byte == FW_FRAME_MAGIC0) {
            frame_buf[frame_pos++] = byte;
        }
        // 不是 0x46 就丢弃 (这过滤掉了 Console 日志)
        return;
    }

    frame_buf[frame_pos++] = byte;

    if (frame_pos >= FW_FRAME_LEN) {
        // 收到完整 8 字节帧
        bool done = fw_info_handle_frame(frame_buf);
        if (done) {
            fw_info_parse(fw_payload, fw_payload_len);
        }
        frame_pos = 0;
    }
}
```

## 7. 接收时序

```
ESP32-C6 上电
  │
  ├─ ~500ms 启动初始化
  │
  ├─ UART0 就绪
  │
  ├─ 发送固件信息帧 (15~18 帧 × 8 字节 = ~12ms @115200)
  │   Frame[0]: 0x46 0x49 0xC0 ...  ← FIRST
  │   Frame[1]: 0x46 0x49 0x81 ...  ← MIDDLE, seq=1
  │   ...
  │   Frame[N]: 0x46 0x49 0x51 ...  ← LAST, seq=17
  │
  ├─ start_done_signal_task()
  │
  ├─ 发送 "C6_DONE\n" × 20 次 (间隔 100ms, 持续约 2秒)
  │
  └─ 进入正常运行
```

**关键点**：RP2350 在收到 `C6_DONE` 之前就应该已经收到全部固件信息帧。如果 RP2350 启动较晚，可能丢失前面的帧。建议：
- 使用空闲超时作为回退检测：如果连续 50ms 未收到 `I` 帧新数据，且之前收到过 FIRST 帧，则视为传输中断，丢弃当前序列。
- 或在 RP2350 与 ESP32-C6 之间加入复位同步信号。

## 8. 完整示例 Payload

假设 ESP32-C6 固件使用 ESP-IDF v5.5.1：

```
SDK=5.5.1
Plat=esp32c6
Board=fightpad12slim_c6_ble_hid
CPU=RISC-V
```

共 68 字节（不含末尾 `\0`），需要 `ceil(68/4) = 17` 帧传输。

## 9. 注意事项

1. **Console 日志干扰**：UART0 TX 上有 ESP-IDF 启动日志，都是可打印 ASCII 文本。只需同步 `0x46` 帧头即可过滤。
2. **XOR 校验**：必须验证，Console 日志中可能偶然出现 `0x46` 开头的行（比如打印了内存地址 `0x46xxxxxx`），但后续 7 字节不会通过 XOR 校验。
3. **payload 末尾填充**：最后一帧不满 4 字节的部分用 `0x00` 填充，解析时应去除尾部的 `0x00`。
4. **超时保护**：建议加超时（如 200ms），防止在接收中途因为某些原因丢帧导致状态机卡住。
5. **上电时序**：RP2350 应在 ESP32-C6 上电前或同时准备好接收 UART 数据，否则可能丢失开头的帧。

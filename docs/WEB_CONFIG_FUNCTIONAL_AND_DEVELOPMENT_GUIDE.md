# FIGHTPAD Web Config 功能与二次开发指南

本文针对当前 `Fightpad12Slim` 分支，说明 Web Config 已经实现了哪些功能、数据如何在浏览器与固件之间流动、修改时应定位哪些文件，以及量产时需要注意什么。

本文依据当前仓库源码整理。上游 Web Config 仍会持续演进，因此后续增加页面或接口时，也应同步更新本文。

## 1. 先给出结论

Web Config 不是需要安装到电脑上的独立软件，也不是从互联网下载配置的网页。它由两部分组成：

- 浏览器端：React 单页应用，源码位于 `www/src/`。
- 设备端：RP2350 进入配置模式后，通过 USB 模拟一个网络适配器，并在 `192.168.7.1` 运行 lwIP HTTP 服务器。

生产版网页会被编译、转换成 C 数组并链接进 UF2：

```text
www/src、www/public
        │
        ▼
Vite 生成 www/build
        │
        ▼
www/makefsdata.js
        │
        ▼
lib/httpd/fsdata.c
        │
        ▼
与固件一起编译进 UF2
```

因此，修改网页源码后如果没有重新生成 `lib/httpd/fsdata.c` 并重新编译、烧录正确的 UF2，设备中显示的仍然是旧网页。

## 2. 现有文档情况

仓库原有资料只能回答一部分问题：

- `docs/ARCHITECTURE.md` 只简要说明 Web Config 是“lwIP HTTPD + React SPA + JSON REST API”。
- `www/README.md` 主要介绍前端开发与构建命令，没有列出页面功能、保存流程和量产边界。
- `docs/CODE_READING_ROADMAP.md` 给出了配置从板级默认值到 Flash 的总体链路，但没有展开 Web Config。

所以本文补充的是“功能清单 + 实现链路 + 修改入口”，而不是替代前端开发命令说明。

## 3. 怎样进入和访问 Web Config

### 3.1 设备启动方式

当前通用固件代码支持以下方式：

- 开机时检测到 `S2`：进入 Web Config。
- 运行过程中长按 Web Config 重启组合键：在游戏手柄模式与 Web Config 模式之间切换。当前通用组合键掩码是 `S2 + B3 + B4`。
- 网页右上角的重启菜单：可以重启到游戏手柄模式、Web Config 模式或 USB BOOTSEL 模式。
- 固件事先写入 `System::BootMode::WEBCONFIG`：下次启动直接进入 Web Config。

板级菜单可以提供额外入口或提示，但最终都会让固件以 `INPUT_MODE_CONFIG` 启动。

“强制设置模式”可以锁定按键切换输入模式、锁定进入 Web Config，或同时锁定两者。量产固件启用这项限制前，必须保留可恢复入口。

### 3.2 电脑与设备之间是什么连接

进入 Web Config 后，设备使用 TinyUSB 网络类：

- Windows 使用 RNDIS。
- macOS 使用 CDC-ECM。
- Linux 可以使用相应的 USB 网络接口。

设备端地址固定为：

```text
http://192.168.7.1
```

设备内置 DHCP 服务，会给电脑的 USB 网络接口分配 `192.168.7.2`、`.3` 或 `.4`。它还启动本地 DNS 和 HTTP 服务，但这个 USB 网络没有互联网网关。

如果浏览器打不开，优先检查：

1. 设备屏幕是否已经显示 `http://192.168.7.1`。
2. Windows“网络适配器”中是否出现新的 RNDIS/USB 网络设备。
3. 电脑该接口是否取得 `192.168.7.x` 地址。
4. 浏览器是否明确使用 `http://`，而不是自动改成 `https://`。
5. VPN、代理、防火墙是否拦截本地 USB 网段。
6. 当前烧录的 UF2 是否包含最新的 `lib/httpd/fsdata.c`。

## 4. 总体实现架构

```text
浏览器 React 页面
  www/src/Pages、Components、Addons
                │
                ▼
前端接口封装
  www/src/Services/WebApi.js
  www/src/Services/Http.ts
                │  HTTP JSON
                ▼
USB RNDIS / CDC-ECM，192.168.7.1
  lib/rndis/rndis.c
                │
                ▼
lwIP HTTPD + 路由/API
  src/webconfig.cpp
                │
                ▼
Storage 中的 Config protobuf 对象
  src/storagemanager.cpp
  proto/config.proto
                │
                ▼
ConfigUtils 序列化 + FlashPROM
  src/config_utils.cpp
  设备 Flash 配置区
```

关键启动链路：

1. `src/gp2040.cpp` 的 `getBootAction()` 判断是否进入 Web Config。
2. 配置模式使用 `NetDriver`，并在 `GP2040::run()` 中调用 `rndis_init()`。
3. `lib/rndis/rndis.c` 初始化 lwIP、DHCP、DNS 和 HTTPD。
4. 浏览器请求静态页面或 `/api/...`。
5. `src/webconfig.cpp` 的 `fs_open_custom()` 将 API 路径交给对应处理函数，或者为 React 路由返回 `index.html`。

配置模式会跳过普通 Core0 Add-on 处理，主循环主要服务 USB 网络、按键读取、保存和重启请求。它不是正常游戏手柄工作模式。

## 5. 页面功能清单

### 5.1 首页 `/`

首页用于查看设备状态，不直接修改配置：

- 当前固件版本、构建版本和构建类型。
- 当前 BoardConfig 名称与板级配置文件名。
- RP2040/RP2350 架构信息。
- Flash 使用量和堆内存使用量。
- 访问 GitHub 获取上游最新版本。

注意：首页的“最新版本”依赖外部 GitHub 网络。USB 配置网络本身没有互联网网关，因此这项查询可能失败。当前代码把两个本地设备接口和 GitHub 请求放在同一个 `Promise.all` 中，GitHub 请求失败时，首页本地系统信息也会一起进入错误状态；但其他本地配置接口仍然可以工作。

### 5.2 设置 `/settings`

这是手柄核心行为配置页，主要包括：

- 输入协议/主机模式，例如 XInput、Switch、PS3、通用 HID、键盘、PS4、PS5 和多种迷你主机模式。
- D-Pad 输出模式：数字方向键、左摇杆或右摇杆。
- SOCD 清理模式：上优先、中性、后输入优先、先输入优先或关闭。
- 当前配置档位、输入去抖和迷你菜单输入设置。
- PS4、PS5、Xbox 等认证方式与相关密钥/USB/I2C 设置。
- 功能键和热键组合，例如切换方向模式、SOCD、配置档、重启模式等。
- 键盘按键映射。
- 高级 USB 描述符覆盖，包括产品名、厂商名、版本、VID 和 PID。
- 强制设置模式，用于限制开机输入模式切换和 Web Config 入口。

主要接口：

- `getGamepadOptions` / `setGamepadOptions`
- `getKeyMappings` / `setKeyMappings`
- `setPS4Options`

### 5.3 GPIO 引脚映射 `/pin-mapping`

该页面负责把物理 GPIO 映射到游戏动作：

- 查看基础档位的 GPIO 功能。
- 捕获当前按下的物理引脚。
- 查看已被固件或外设占用的引脚。
- 配置多个 Profile，设置名称、启用状态并复制基础映射。
- 映射方向键、主要按键、附加按键、宏触发、Turbo、SOCD、模拟方向、菜单导航等动作。

主要接口：

- `getPinMappings` / `setPinMappings`
- `getProfileOptions` / `setProfileOptions`
- `getUsedPins`
- `getHeldPins` / `abortGetHeldPins`

板级保留引脚和 Add-on 已占用引脚不能随意复用。新增硬件功能时，应同时检查 `configs/Fightpad12Slim/BoardConfig.h` 和 Web Config 的“已用引脚”结果。

### 5.4 外设映射 `/peripheral-mapping`

该页面统一配置 RP2040/RP2350 硬件外设块及其引脚：

- I2C0、I2C1：SDA、SCL 和速度。
- SPI0、SPI1：RX、TX、SCK、CS 等引脚。
- USB Host：数据和 5V 使能相关配置。

主要接口：

- `getPeripheralOptions` / `setPeripheralOptions`
- `getI2CPeripheralMap`

这里定义的是“总线资源”，Add-on 页面定义的是“哪个功能使用这条总线”。两处配置必须一致。

### 5.5 LED 配置 `/led-config`

用于设置 WS2812、玩家指示灯和外壳灯：

- WS2812 数据引脚、颜色格式和布局。
- 每个按键的 LED 数量与按键顺序。
- 最大亮度和亮度调整步进。
- 玩家编号 LED。
- Case/Ambient LED。
- USB 挂起时是否关闭 LED。

主要接口：`getLedOptions` / `setLedOptions`。

### 5.6 自定义 LED 主题 `/custom-theme`

用于为每个逻辑按键设置正常颜色和按下颜色，并提供布局预览。

主要接口：`getCustomTheme` / `setCustomTheme`。

该页面修改的是设备 LED 主题；它与网页本身的颜色主题不是一回事。

### 5.7 显示屏配置 `/display-config`

用于配置 OLED/单色显示屏及启动画面：

- 是否启用显示屏、I2C 外设块、SDA/SCL、地址和速度。
- 翻转、镜像和反色。
- 左右按键布局和自定义坐标。
- 启动画面模式、持续时间和自定义图片。
- 屏幕保护模式、超时和电源管理。
- 输入历史显示。
- 输入模式、Turbo、D-Pad、SOCD、宏和 Profile 状态栏显示。

主要接口：

- `getDisplayOptions` / `setDisplayOptions`
- `setPreviewDisplayOptions`
- `getSplashImage` / `setSplashImage`
- `getButtonLayouts` / `getButtonLayoutDefs`

显示配置页面会把编辑值发送到“预览”接口。预览会修改当前 RAM 中的显示配置，但不立即写 Flash；点击保存后才触发持久化。

启动图不是普通网页图片。上传后，前端把单色像素数据发送到固件，固件保存为显示配置的一部分。量产统一启动图时，更适合将默认字模写进板级配置，而不是逐台上传。

### 5.8 Add-on 配置 `/add-ons`

当前页面注册了以下可选功能：

- BOOTSEL 按键、板载 LED。
- 模拟摇杆、ADS1219、ADS1256。
- Turbo、反向输入、双方向输入、Tilt、SOCD Slider。
- 蜂鸣器。
- Wii、SNES、TG16 外设输入。
- Focus Mode、键盘 Host、游戏手柄 USB Host。
- 旋转编码器、PCF8575 GPIO 扩展。
- DRV8833 震动。
- Reactive LED。
- Hall Effect Trigger。
- Fightpad ESP32 Proxy。

主要接口：

- `getAddonsOptions` / `setAddonsOptions`
- `getWiiControls` / `setWiiControls`
- `getReactiveLEDs` / `setReactiveLEDs`
- `getExpansionPins` / `setExpansionPins`
- `getHETriggerVoltage`、`setHETriggerOptions`
- `getHETriggerCalibrations` / `setHETriggerCalibrations`

页面出现某个 Add-on 不代表当前板子已经接了对应硬件。应以 `Fightpad12Slim` 原理图、`BoardConfig.h` 默认值和实际焊接为准。

### 5.9 宏配置 `/macro`

用于配置输入宏，包括：

- 宏触发按键/引脚。
- 按下、按住重复、切换等触发方式。
- 每一步按键组合与持续时间。
- 时间单位和执行行为。
- 是否独占、是否允许中断等选项。

主要接口：`getMacroAddonOptions` / `setMacroAddonOptions`。

### 5.10 数据备份和恢复 `/backup`

网页可以导出、导入 `.gp2040` JSON 文件。当前界面按类别处理：

- Display
- Splash
- Gamepad
- Keyboard
- LED
- LED Theme
- Macros
- Pins
- Profiles
- HE Trigger calibration
- Add-ons

导入时，前端把文件内容与设备当前类别配置合并，再逐个调用对应的 setter。

重要限制：这不是严格意义上的完整 Flash 镜像。当前类别列表没有覆盖所有独立配置，例如 Peripheral Mapping。固件虽提供 `/api/getConfig` 和 `/api/setConfig` 全量接口，但当前网页备份页没有使用它们。量产工具或完整备份优化应优先解决这个差异。

### 5.11 重置设置 `/reset-settings`

调用 `/api/resetSettings` 后会清除 EEPROM/FlashPROM 配置区并重启。下次启动时，未保存的项目重新从板级默认值和固件默认值初始化。

此操作会丢失用户配置，量产测试流程中应明确它发生在写入出厂设置之前还是之后。

### 5.12 页面顶栏

顶栏还提供：

- 返回首页。
- 配置页面导航。
- 语言选择。
- 按键名称样式选择，例如 FIGHTPAD/PS3 等。
- 重启到游戏手柄、Web Config 或 BOOTSEL。

按键名称样式保存在浏览器 `localStorage` 的 `buttonLabels` 项中。它只改变网页如何称呼按键，不会写入设备 Flash，也不会改变 USB 输入协议。

## 6. 配置如何保存和生效

### 6.1 普通保存链路

```text
点击页面“保存”
  → WebApi.js 发送 JSON POST
  → src/webconfig.cpp 解析 JSON
  → 修改 Storage::getInstance() 中的 Config
  → 触发 GPStorageSaveEvent(true)
  → GP2040 主循环调用 Storage::save(true)
  → ConfigUtils 使用 nanopb 序列化
  → FlashPROM 写入配置区
```

`proto/config.proto` 是持久化结构的核心定义。`ConfigUtils::initUnsetPropertiesWithDefaults()` 会为旧配置或空配置补齐默认值，其中一部分默认值来自 `BoardConfig.h`。

### 6.2 哪些修改可能需要重启

保存成功只表示配置已经写入 Flash，不代表所有运行模块都已经重新初始化。

- 页面文字、语言和按键标签：浏览器中立即生效。
- 显示预览：当前配置模式中可立即看到。
- Profile 引脚映射：运行时有专门的重新初始化路径。
- USB 输入协议、USB 描述符、外设块、某些 Add-on 和硬件引脚：通常应重启后验证。
- 重置设置：代码会自动重启。

对新功能最稳妥的设计是让后端明确返回“已保存”和“需要重启”状态，而不是让用户猜测。

### 6.3 板级默认值与已保存配置的优先级

量产时必须区分两类设备：

- Flash 配置区为空：固件会使用 `BoardConfig.h` 和代码中的默认值。
- Flash 已有旧配置：已保存字段通常优先于新的板级默认值。

所以只修改 `BoardConfig.h` 并重新烧录 UF2，不保证已经保存过配置的样机立刻采用新默认值。量产流程应选择一种明确策略：

1. 新板首次烧录，确保配置区为空，再由固件生成出厂默认值。
2. 烧录后执行一次“重置设置”，再验证默认值。
3. 增加配置版本迁移代码，对旧字段进行定向升级。
4. 使用专用工装调用全量配置接口写入统一出厂配置。

对于所有机器都相同的品牌、按键、引脚、启动图和功能开关，优先采用代码/板级默认值；不要依赖人工逐台打开网页。

## 7. 静态网页是怎样进入 UF2 的

### 7.1 正常构建链

`www/package.json` 中的生产构建脚本会执行：

```text
npm run build-proto
  → npx vite build
  → npm run makefsdata
```

最后一步由 `www/makefsdata.js` 把 `www/build` 转换为 `lib/httpd/fsdata.c`。

根 `CMakeLists.txt` 在未启用 `SKIP_WEBBUILD` 时，会在配置阶段执行：

```text
npm ci
npm run build
```

然后固件编译会链接新生成的 `fsdata.c`。

### 7.2 `SKIP_WEBBUILD` 的影响

如果环境变量或 CMake 缓存启用了 `SKIP_WEBBUILD`，固件构建会跳过前端生成，直接使用仓库里当时已有的 `lib/httpd/fsdata.c`。

这会造成最常见的现象：React 源码已经改了，UF2 也重新生成了，但设备网页没有任何变化。

排查时应核对：

- `www/src` 修改时间。
- `lib/httpd/fsdata.c` 是否在之后更新。
- 烧录的 UF2 是否来自当前仓库的正确 `build` 目录。
- 设备重连后是否仍被浏览器缓存旧页面；必要时强制刷新或使用无痕窗口。

### 7.3 不要手工修改 `fsdata.c`

`lib/httpd/fsdata.c` 是生成文件，内容包含压缩/编译后的 HTML、JS、CSS 和图片。直接改它很难维护，下一次前端构建也会覆盖修改。

正确入口始终是：

- 页面：`www/src/Pages/`
- 公共组件：`www/src/Components/`
- 文案：`www/src/Locales/`
- 静态资源：`www/public/`
- API：`www/src/Services/WebApi.js`

## 8. 品牌和 Logo 修改位置

FIGHTPAD 量产品牌通常涉及以下文件：

| 目标 | 修改位置 |
|---|---|
| 浏览器标题和 description | `www/index.html` |
| PWA 名称和图标 | `www/public/manifest.json` |
| 顶栏品牌 | `www/src/Components/Navigation.jsx`、`Navigation.scss` |
| 通用品牌文字 | `www/src/Locales/*/Common.jsx` |
| 首页欢迎语 | `www/src/Locales/*/HomePage.jsx` |
| 右上角按键样式名称 | `www/src/Data/Buttons.js` |
| 按键样式默认选择 | `www/src/Services/Storage.js` |

当前 lwIP 静态文件服务没有完整处理 SVG MIME 类型。仓库原说明明确建议网页图片使用 PNG/JPG。用于 favicon、PWA 图标或 `<img>` 的量产资源应优先转换成 PNG，而不是只在桌面 Vite 开发服务器中验证 SVG。

还有两个容易混淆的 Logo：

- Web 顶栏 Logo：普通网页资源或 CSS/文字。
- OLED 开机 Logo：单色像素数组，存放在固件显示配置中。

两者的转换、尺寸和保存路径完全不同。

## 9. 后端 API 分类

`src/webconfig.cpp` 当前注册的主要接口如下。方法列表示当前前端的调用方式；lwIP 的自定义文件处理本身并不是完整的 REST 框架。

| 分类 | 读取/动作 | 写入 |
|---|---|---|
| 系统信息 | `getFirmwareVersion`、`getMemoryReport` | `reboot`、`resetSettings` |
| 显示 | `getDisplayOptions`、`getSplashImage`、`getButtonLayouts`、`getButtonLayoutDefs` | `setDisplayOptions`、`setPreviewDisplayOptions`、`setSplashImage` |
| 手柄 | `getGamepadOptions`、`getKeyMappings` | `setGamepadOptions`、`setKeyMappings`、`setPS4Options` |
| 引脚/Profile | `getPinMappings`、`getProfileOptions`、`getUsedPins`、`getHeldPins`、`abortGetHeldPins` | `setPinMappings`、`setProfileOptions` |
| LED | `getLedOptions`、`getCustomTheme`、`getReactiveLEDs` | `setLedOptions`、`setCustomTheme`、`setReactiveLEDs` |
| Add-on | `getAddonsOptions`、`getWiiControls` | `setAddonsOptions`、`setWiiControls` |
| 外设 | `getPeripheralOptions`、`getI2CPeripheralMap` | `setPeripheralOptions` |
| 扩展/霍尔 | `getExpansionPins`、`getHETriggerVoltage`、`getHETriggerCalibrations` | `setExpansionPins`、`setHETriggerOptions`、`setHETriggerCalibrations` |
| 宏 | `getMacroAddonOptions` | `setMacroAddonOptions` |
| 模拟摇杆校准 | `getJoystickCenter`、`getJoystickCenter2` | 页面直接读取校准结果 |
| 全量配置 | `getConfig` | `setConfig` |

调试构建还会提供 `/api/echo`。

## 10. 修改功能时应该改哪些文件

### 10.1 只改网页布局或文字

通常只需要：

- 对应 `www/src/Pages/` 或 `www/src/Components/`。
- 对应 `www/src/Locales/<语言>/`。
- SCSS 样式文件。
- 最后重新生成 `fsdata.c` 并编译 UF2。

这种修改不需要改 `proto/config.proto` 或固件保存代码。

### 10.2 在已有配置中增加一个页面控件

如果后端 JSON 已经包含该字段：

1. 在页面的初始值与校验 Schema 中加入字段。
2. 把控件接入 Formik 表单。
3. 检查 `WebApi.js` 的请求清洗是否会删掉该字段。
4. 补齐所有语言文案。
5. 在模拟服务器中补充测试数据。

### 10.3 增加一个可持久化的新配置字段

推荐顺序：

1. 在 `proto/config.proto` 中增加字段，保持字段编号唯一，禁止复用历史编号。
2. 在 `src/config_utils.cpp` 中增加默认值、旧配置兼容和必要的迁移逻辑。
3. 如果是 Fightpad 固定默认值，在 `configs/Fightpad12Slim/BoardConfig.h` 增加宏或板级默认值。
4. 在 `src/webconfig.cpp` 的 getter/setter 中增加 JSON 字段。
5. 确认 setter 会触发保存事件，必要时还要请求重启。
6. 在 `www/src/Services/WebApi.js` 和对应页面中增加字段。
7. 更新 `www/server/app.js` 模拟接口和 Postman 集合。
8. 更新备份/恢复类别，避免新字段无法导出。

CMake 会通过 `compile_proto.cmake` 调用 nanopb 生成 `config.pb.c/h` 和 `enums.pb.c/h`。这些输出位于构建目录，不应手工编辑。

### 10.4 新增一个 API

当前真实入口不是旧文档所写的 `src/webserver.cpp`，而是 `src/webconfig.cpp`：

1. 在 `src/webconfig.cpp` 实现处理函数。
2. 在 `handlerFuncs` 或 `handlerFuncsWithStatusCode` 表中注册 `/api/...` 路径。
3. POST 请求从全局 POST 缓冲区读取 JSON，注意载荷上限。
4. 在 `www/src/Services/WebApi.js` 增加前端封装。
5. 在 `www/server/app.js` 增加本地模拟接口。
6. 更新 `www/server/docs/GP2040-CE.postman_collection.json`。
7. 明确定义失败状态、JSON 错误结构和是否需要重启。

### 10.5 新增一个页面

1. 在 `www/src/Pages/` 创建页面。
2. 在 `www/src/App.jsx` 注册 React Router 路由。
3. 在 `www/src/Components/Navigation.jsx` 添加菜单入口。
4. 在 `src/webconfig.cpp` 的 `spaPaths` 中加入同一路径，使用户直接刷新子页面时也能得到 `index.html`。
5. 添加多语言文案。

当前 `/playground` 已在 React 中注册，但没有加入 `spaPaths`，也没有正常导航菜单入口。它更接近开发页面；直接访问或刷新该路径可能无法像其他页面一样回退到 `index.html`。

## 11. 当前代码中值得优先优化的地方

### 11.1 第一优先级：保证量产一致性

- 确认所有品牌资源都从源码进入 `fsdata.c`，禁止手工替换生成文件。
- 把统一启动图、引脚、Add-on 开关等改为 Fightpad 板级默认值。
- 定义“空配置区、旧配置区、恢复出厂设置”三种情况下的预期结果。
- 为出厂 UF2 和 `fsdata.c` 记录版本或哈希，避免烧错目录中的旧文件。

### 11.2 第二优先级：补全备份和恢复

- 让备份页覆盖 Peripheral、Expansion 等所有独立类别，或直接建立带版本号的全量配置格式。
- 导入前显示固件/板型兼容性。
- 导入后汇总每个类别的成功、失败和重启要求。

### 11.3 第三优先级：提高 API 可靠性

- `Http.ts` 当前直接解析 JSON，没有先检查 `response.ok`，错误响应容易表现为普通数据或解析异常。
- 为所有 setter 统一返回 `{ success, saved, rebootRequired, error }`。
- 对输入范围、缺失字段和超长 POST 提供明确的 4xx 响应。
- 避免前端只显示通用“保存失败”，应指出具体字段或模块。

### 11.4 第四优先级：降低维护成本

- `src/webconfig.cpp` 已集中大量配置转换逻辑，可以按系统、显示、LED、引脚、Add-on 拆分模块。
- 让 API Schema、前端类型和 protobuf 字段之间建立可检查的对应关系。
- 把 `App.jsx` 路由与固件 `spaPaths` 的重复列表集中生成或至少加入一致性测试。
- 修正旧文档路径，避免新功能继续添加到不存在的 `webserver.cpp`。

### 11.5 第五优先级：离线和缓存体验

- 首页最新版本查询失败时应明确显示“离线”，不要影响本地信息。
- 将首页本地固件/内存请求与 GitHub 版本请求拆开，避免一个外部请求让整组 `Promise.all` 失败。
- 为生产静态资源建立清晰的缓存版本策略。
- 品牌图标全部使用设备 HTTPD 能正确返回 MIME 的格式。

## 12. 建议的修改验证清单

每次改 Web Config，至少完成以下检查：

- [ ] 桌面开发模式下页面能打开，表单没有控制台错误。
- [ ] 新字段能从模拟接口读取和保存。
- [ ] 连接真实设备时，GET 返回值与页面显示一致。
- [ ] 保存后重新进入页面，值仍然存在。
- [ ] 设备断电重启后，值仍然存在。
- [ ] 需要重启的功能在重启后实际生效。
- [ ] 备份文件包含新字段，恢复后结果一致。
- [ ] `lib/httpd/fsdata.c` 的更新时间晚于网页源码修改时间。
- [ ] 烧录的是当前仓库正确构建目录生成的 UF2。
- [ ] `http://192.168.7.1` 的根页面和所有子页面刷新都正常。
- [ ] Windows RNDIS 网络重新枚举后仍能访问。
- [ ] 无痕窗口中品牌、文案和图标也正确，排除浏览器缓存影响。
- [ ] 新板、已有配置的旧板、恢复出厂设置后的板各测试一次。

## 13. 推荐阅读顺序

如果要在本文基础上开始修改，建议按以下顺序阅读源码：

1. `www/src/App.jsx`：有哪些页面和路由。
2. `www/src/Components/Navigation.jsx`：页面入口和顶栏功能。
3. 目标页面文件：表单字段和交互。
4. `www/src/Services/WebApi.js`：页面请求的 API。
5. `src/webconfig.cpp`：JSON 与固件配置的转换。
6. `src/storagemanager.cpp`：配置对象和保存动作。
7. `src/config_utils.cpp`：默认值、兼容、序列化和 Flash。
8. `proto/config.proto`：持久化字段定义。
9. `configs/Fightpad12Slim/BoardConfig.h`：Fightpad 出厂默认值。
10. `www/makefsdata.js` 与 `CMakeLists.txt`：网页怎样进入 UF2。

沿着这条链路修改，可以避免只改了页面、只改了默认值，或者只改了生成文件但实际设备没有生效的问题。

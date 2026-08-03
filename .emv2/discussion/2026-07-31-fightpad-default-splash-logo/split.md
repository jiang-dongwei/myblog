# 需求拆分

讨论ID：`2026-07-31-fightpad-default-splash-logo`

步骤编号：`S26`

## 1. Fightpad12Slim 板级默认配置

- 类型：配置。
- 将量产 Logo 固化为 Fightpad12Slim 的 `DEFAULT_SPLASH`，不影响其他 GP2040-CE 板卡。

## 2. OLED 静态启动图

- 类型：显示。
- 使用用户提供的 `zimo.TXT`：128×64、1 bpp、逐行、MSB-first，共 1024 字节。

## 3. 量产配置状态

- 类型：存储。
- 新设备直接使用固件默认图；已有持久化 Web Config 的设备需要恢复出厂或擦除配置后才能看到新的默认图。

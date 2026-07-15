# 硬件对齐

- 电量计：BQ27220，软件 I2C GP25=SCL、GP26=SDA、GP27=GPOUT。
- 按键灯：GP22，12 颗 WS2812，由 `FightpadAmbientLEDAddon` 的 `neopico_gp22` 驱动。
- 环境灯：GP40，19 颗 WS2812，由 `FightpadAmbientLEDAddon` 的 `neopico` 驱动。
- 新增硬件：无。
- 新增引脚：无。
- 资源冲突：通用 `NeoPicoLEDAddon` 在 `FIGHTPAD12SLIM_AMBIENT_OWNS_GP22=1` 时禁用，当前两条灯链均由 Fightpad addon 统一拥有。

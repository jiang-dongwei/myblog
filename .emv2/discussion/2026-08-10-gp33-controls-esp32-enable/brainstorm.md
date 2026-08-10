# 方案推演

## 采用：独立板级 EN 跟随路径

- 在FightpadESP32ProxyAddon中增加板级EN引脚配置。
- setup阶段先预装目标输出电平再切换为输出，减少错误高脉冲。
- process阶段读取现有`isBluetoothTransportSelected()`，仅在模式变化时更新GP34。
- GP33采用候选值、稳定值和候选起始时间的非阻塞30ms消抖，不阻塞主循环。
- USB报告路径与ESP32 Proxy各自维护同参数消抖状态，避免跨核心共享可变状态；两条路径的确认时间最多相差一个调度周期。

## 未采用

- 不复用`resetPin`：该字段属于WebConfig/DTR-RTS复位下载路径，未来可能产生所有权冲突。
- 不在ESP32-C6固件里实现：GP34直接接C6 EN，C6在EN低时无法执行软件休眠代码。
- 不单独反转GP34输出而保留旧的传输判定：否则C6运行状态与RP2350是否发送BT输入帧相反。

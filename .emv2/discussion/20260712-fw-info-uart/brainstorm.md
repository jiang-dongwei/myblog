# 头脑风暴

## 难点1: 多帧协议设计
- 选择方案A: Byte2高2位=flag(11首/10中/01尾/00单), 低6位=seq
- Byte3~6: 4字节payload
- 约13帧发送52字节

## 难点2: 数据格式
- 文本 `key=value\n` 格式
- SDK运行时获取, Plat/Board/CPU编译期宏

## 难点3: CPU架构名映射
- `riscv` → `RISC-V`, `xtensa` → `Xtensa`
- 条件编译宏实现

## 难点4: 发送时机
- UART初始化后立即发送，先固件信息帧，后C6_DONE

## 难点5: 代码组织
- 方案A: 直接在 main.c 中添加，减少构建系统变更

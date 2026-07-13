# 子流程拆分

## S1-A: 固件信息帧协议与发送函数
- 所属子系统: UART发送
- 开发内容: FW_INFO帧类型宏、多帧flag宏、CPU架构映射、send_fw_info_frame()多帧分包发送
- 前置条件: 无
- 验证方式: 编译通过
- 优先级: 1

## S1-B: 固件信息采集与启动集成
- 所属子系统: 固件信息采集 + 启动流程
- 开发内容: 构建payload文本、start_fw_info_signal()、app_main()集成、CMakeLists.txt添加PROJECT_NAME宏
- 前置条件: S1-A完成
- 验证方式: 实机上电RP2350收到固件信息帧
- 优先级: 2

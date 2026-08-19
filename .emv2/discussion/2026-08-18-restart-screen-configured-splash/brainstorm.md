# 重启页复用配置启动图 - 技术方案

## 图片一致性

- `SplashScreen`的权威图片来源是`getDisplayOptions().splashImage.bytes`。
- `RestartScreen`直接复用同一数据和`128×64、pitch=16`绘制参数，避免复制默认数组或引入第二套Logo。

## 文字可读性

- 先绘制完整启动图，再清除底部48～63像素形成两行文字区。
- 原有BootMode分支和文字内容保持不变。

## 变更边界

- 删除RestartScreen对旧`BitmapScreens.h` Logo的依赖。
- 不修改GPRestartEvent、Web API、看门狗重启或SplashScreen。

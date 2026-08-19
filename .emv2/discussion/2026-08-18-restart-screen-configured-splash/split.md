# 重启页复用配置启动图 - 需求拆分

## 1. 重启页图片来源

- Web Config右上角重启操作继续进入现有`RestartScreen`。
- 将旧内置`bitmapGP2040Logo`替换为Flash配置中的`DisplayOptions.splashImage`。

## 2. 提示文字

- 保留Controller、Web Config、BOOTSEL对应的模式说明。
- 保留`Please Wait`等现有等待文字。

## 3. 兼容范围

- 不修改启动图上传、Flash保存、正常开机Splash时长和重启事件流程。

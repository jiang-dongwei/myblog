# 重启页复用配置启动图 - 需求确认

- 重启页显示当前已经存入`displayOptions.splashImage`的128×64静态图片。
- 新烧录/恢复默认配置的设备使用Fightpad板级默认图；Web Config上传过图片的设备使用上传后的图。
- 图片来源对Controller、Web Config和BOOTSEL重启目标保持一致，不再显示旧GP2040 Logo。
- 底部两行保留原模式说明与`Please Wait`等文字，并使用黑色背景保证可读性。
- 即使正常启动画面被设置为关闭，重启确认过程仍直接读取已保存的图片数据。

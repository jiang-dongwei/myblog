# PS3残留设备类型容错 - 需求确认

- 同一USB硬件档位下Switch、PS4、PS5和Xbox正常，PS3能被PC识别但没有按键输入。
- 实体Controller Type菜单中的八种模式均按普通Gamepad使用，不保留其他模式留下的专业设备子类型。
- PS3继续保留上游明确支持的Gamepad Alternate、Wheel、Guitar和Drum。
- 不修改其他USB驱动，不取消Web Config的专业设备类型功能。
- 用户负责编译、烧录和实机验证，本步骤只做源码修改与非编译静态检查。

用户已于2026-08-10确认按上述方案修改。

# 需求拆分

## 1. GP22 Chase周期

- 类型：灯效时序
- 将普通Light Effect Chase中GP22按键灯链的单个追逐头完整一圈时间由1000ms改为2000ms。
- GP40普通Chase保持2000ms，蓝牙Pairing/Connecting Chase保持50ms/颗。

## 2. OLED空闲休眠

- 类型：显示电源状态
- 保持当前60秒无操作后关闭OLED供电的阈值。

## 3. 任意按键唤醒

- 类型：输入活动检测
- GP2～GP20任意游戏按键按下都刷新OLED活动时间并唤醒屏幕。
- 保留GP30/GP31/GP32与蓝牙状态事件的现有唤醒能力。

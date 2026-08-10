# 硬件对应

- GP22：12颗LED，1000ms/圈，平均约83.3ms跨过一颗灯。
- GP40：19颗LED，2000ms/圈，平均约105.3ms跨过一颗灯。
- 两链继续由同一个 FightpadAmbientLEDAddon 刷新，不增加定时器或GPIO资源。

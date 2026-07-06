# 头脑风暴

## 难点1: 架构集成方式

**分析**: 现有显示系统用 `DisplayMode` 枚举 + `GPScreen` 子类 + 手柄按键导航。拨轮菜单完全不同 — 用物理编码器输入 (不经过手柄输入管线)，有独立的多层级结构。

**方案对比**:

| 维度 | A: 扩展现有DisplayMode | B: 独立GPAddon + 内置渲染 |
|------|----------------------|--------------------------|
| DisplayMode | 新增 SCROLL_WHEEL_MENU | 不碰现有枚举 |
| GPScreen | 新增 ScrollWheelMenuScreen | 不需要，直接调GPGFX |
| 导航输入 | 需要把编码器事件注入现有机制 | 自己读GPIO，不经过游戏手柄管道 |
| GPIO仲裁 | 需要 DisplayAddon 感知 | 通过全局flag通信 |
| 与DIP共存 | DisplayAddon 不管理DIP | 直接设置标志位暂停DIP |
| 侵入性 | 需改DisplayAddon + GPGFX_UI_screens.h | 新增文件，FightpadAmbientLEDAddon 加一行检查 |

**建议**: 方案B — 新建 `ScrollWheelMenuAddon` (GPAddon)，独立管理菜单渲染和编码器输入，通过全局标志位与 FightpadAmbientLEDAddon 通信。

---

## 难点2: GPIO30-32 共享仲裁

**分析**: FightpadAmbientLEDAddon 和 ScrollWheelMenuAddon 都要读 GPIO30-32，需要互斥。

**方案**: 一个 `extern volatile bool g_scrollWheelMenuActive` 全局标志:
- `ScrollWheelMenuAddon::process()` 中管理状态切换 (GP30长按3s)
- `FightpadAmbientLEDAddon::process()` 开头检查: `if (g_scrollWheelMenuActive) return;` — 跳过DIP逻辑但保持LED渲染
- 菜单退出时设置 `g_scrollWheelMenuActive = false`，DIP自动恢复

**风险**: GPIO初始化 (pull-up, direction) 两个 addon 重复调 — 无副作用，`gpio_init` 幂等。

---

## 难点3: 正交编码器读取

**分析**: GP31(A)/GP32(B) 是正交编码器。每一步旋转产生 A和B 的特定跳变序列。

**方案**: 状态机解码:
```
prevA=1, prevB=1
    ↓
A↓=0 (B仍=1) → 顺时针
B↓=0 (A仍=1) → 逆时针
    ↓
prevA/B更新, 累积delta
```
每个 process() 周期读取当前A/B状态，与上次比较，检测跳变 → 更新位置累积器。返回 delta 给菜单层用于滚动。

- 去抖: 50ms (process 调用频率足够)
- 灵敏度: 每个完整步(4个跳变) = 菜单移动1项

---

## 难点4: GP30长短按检测

**分析**: 同一按键区分短按(选择/返回)和长按(进入/退出菜单)。

**方案**: 
```
GP30按下:
  - 记录 pressStartTime
  - 如果 release 且 duration < 3000ms → 短按事件
  - 如果持续按下 ≥ 3000ms → 长按事件 (仅触发一次，用标志防重入)
  - 长按后等待释放才重置状态

仅在MODE_NORMAL时检测长按；仅在MODE_MENU时检测长按
短按行为取决于当前层级(有子菜单→进入，叶子→返回上一层)
```

---

## 难点5: 菜单渲染

**分析**: 128x64 OLED，需显示菜单列表 + 选中高亮 + 滚动。

**方案**: 直接调用 GPGFX API (drawText), 不创建 GPScreen 子类:
- 每行高: 10px → 最多显示6行 (60px)
- 当前选中: 反色条 (drawRect 填充 + 白色文字)
- 右侧滚动指示器: 竖条 + 滑块位置
- 颜色选择页 (Level 2): 显示色块名 + 小色块方块

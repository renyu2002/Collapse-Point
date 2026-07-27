# 实现路线图（C++ 优先）

本文件是 **下一阶段** 的执行 checklist。当前仓库仅固化文档，不创建 `.uproject`、不编写蓝图玩法。

原则与规格见 [TECH_SPEC.md](TECH_SPEC.md)；关卡验收见 [VERTICAL_SLICE.md](VERTICAL_SLICE.md)。

---

## Phase 0 — 工程脚手架

- [ ] 新建 UE5 第三人称 **C++** 项目（建议名 `CollapsePoint`）
- [ ] 删除射击武器、弹匣、准星射击相关代码与输入
- [ ] 保留移动、跳跃、相机；接入 Enhanced Input
- [ ] 确认启动 Map 为空白试验腔（或临时空关卡）

## Phase 1 — 核心奇点

- [ ] 实现 `ISuckable`、`APhysicsObject`
- [ ] 实现 `USingularityAttractComponent` + `ASingularity`（质量、时限、**准星拖动**、崩塌冲量）
- [ ] 实现 `ACollapsePointCharacter`：瞄准 Trace、RMB 按住/松开、甩动加力、冷却、LMB 自身弹跳
- [ ] 玩家轻吸引 + 贴脸风险（最小可玩版本）
- [ ] 白模房间 + 若干物理方块，验证：创造 → 拖动吸入 → 朝准星爆发

**里程碑**：无敌人时，用方块砸墙已经「手痒」。

## Phase 2 — 伤害与敌人

- [ ] 撞击伤害（速度 × 质量 × 系数 vs 阈值）
- [ ] `AEnemyPawn`：走向玩家、接触伤害、可被吸入
- [ ] 调参至：抛射撞墙可击杀轻型敌人

**里程碑**：B 区核心循环可玩。

## Phase 3 — 过关与三区关卡

- [ ] `ATriggerButton` + 开门（A 区）
- [ ] 连续试验腔：A → B → C 几何与道具摆放
- [ ] 杂物堆（哇时刻）、玻璃/箱子等可吸物
- [ ] 重型敌人阈值、爆炸桶、`ADualSwitchShield`（C 区）
- [ ] Checkpoint / 简单死亡重生（可选但建议）

**里程碑**：5–8 分钟跑通垂直切片验收表。

## Phase 4 — 表现与打磨（约 60% 时间）

- [ ] Niagara 漩涡挂到 `ASingularity`
- [ ] `UCollapsePointCameraFX`：色散 + 径向模糊（第 5 秒钩子）
- [ ] 反复调：吸引曲线、崩塌力度、散射角、甩动增益、伤害阈值
- [ ] 让奇点「看起来危险」但不致晕眩
- [ ] 走完 [VERTICAL_SLICE.md](VERTICAL_SLICE.md) 全部验收 ID

## Phase 5 — 打包

- [ ] Development / Shipping 打包为独立程序
- [ ] 无菜单、启动即玩；体积尽量精简（白模 + 少量特效）
- [ ] 冒烟：完整 A→B→C 一遍

---

## 明确不做（全程）

- 剧情、主菜单、存档、成就  
- 复杂动画与复杂 AI  
- 用蓝图实现玩法状态机  

## 文档维护

实现过程中若改参数默认值或类名，同步更新 [TECH_SPEC.md](TECH_SPEC.md)；玩法变更同步 [DESIGN.md](DESIGN.md) / [VERTICAL_SLICE.md](VERTICAL_SLICE.md)。

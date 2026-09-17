# 实现路线图（C++ 优先）

本文件是 **下一阶段** 的执行 checklist。当前仓库仅固化文档，不创建 `.uproject`、不编写蓝图玩法。

原则与规格见 [TECH_SPEC.md](TECH_SPEC.md)；关卡验收见 [VERTICAL_SLICE.md](VERTICAL_SLICE.md)。

---

## Phase 0 — 工程脚手架

- [x] 新建 UE5 第三人称 **C++** 项目（建议名 `CollapsePoint`）
- [x] 删除射击武器、弹匣、准星射击相关代码与输入（模板无枪械，沿用移动跳跃）
- [x] 保留移动、跳跃、相机；接入 Enhanced Input
- [x] 确认启动 Map 为试验腔关卡 → 现用 `Lvl_CollapsePointDemo`

## Phase 1 — 核心奇点

- [x] 实现 `ISuckable`、`APhysicsObject`
- [x] 实现 `USingularityAttractComponent` + `ASingularity`（质量、时限、**准星拖动**、崩塌冲量）
- [x] 实现 `ACollapsePointCharacter`：瞄准 Trace、RMB 按住/松开、甩动加力、冷却、LMB 自身弹跳
- [x] 玩家轻吸引 + 贴脸风险（最小可玩版本）
- [x] 白模房间 + 若干物理方块，验证：创造 → 拖动吸入 → 朝准星爆发（关卡已放 8 个方块；请人工 PIE 手感确认）

**里程碑**：无敌人时，用方块砸墙已经「手痒」。

## Phase 2 — 伤害与敌人

- [x] 撞击伤害（速度 × 质量 × 系数 vs 阈值）
- [x] `AEnemyPawn`：走向玩家、接触伤害、可被吸入
- [x] 调参至：抛射撞墙可击杀轻型敌人

**里程碑**：B 区核心循环可玩。

## Phase 3 — 过关与试验腔

- [x] `ATriggerButton` + 开门（A 区）
- [x] 连续试验腔：A → B → C 几何与道具摆放
- [x] 引力点试验腔：入轨不吞噬、环刮压板、可破坏玻璃/吊物
- [x] 双开关护盾 + 本腔重置（超时/R/死亡）
- [x] Checkpoint / 简单死亡重生

## Phase 3.5 — 轨道塑形与五腔扩展

- [x] 鼠标滚轮在 80–380 cm 间收缩 / 扩张轨道
- [x] 轻物高速绕行；重物低速、高冲击、可触发质量接收器
- [x] 抑制场：阻止引力井生成 / 拖过，允许玩家与脱环物体通过
- [x] B 腔改为“缩环穿孔、扩环双触发”
- [x] C 腔改为“轻物破障、重物开门”
- [x] 新增 D 越场抛射与 E 综合终局
- [x] 五腔全部直接摆入 `Lvl_CollapsePointDemo`，不在 BeginPlay 自动刷关

**里程碑**：白模五腔形成“选物 → 成环 → 塑形 → 拖刮 / 抛射”的完整变化。

## Phase 3.6 — 轨道平面与八腔扩展

- [x] Shift + 滚轮在 0–90° 间旋转轨道平面
- [x] 脉冲抑制场：亮起阻断，熄灭形成运输窗口
- [x] 易碎货物：稳定运输可存活，高速碰撞会损坏，重置后恢复
- [x] 新增 F 纵缝立环、G 脉冲运输、H 组合编舞
- [x] F/G/H 直接摆入 `Lvl_CollapsePointDemo`
- [x] 无窗口自动测试扩展到 A–H，并记录轨迹、最大倾角和机制断言

**里程碑**：白模八腔覆盖轨道大小、轨道平面、质量、时机、物性与越场抛射。

## Phase 3.7 — "翻面"动词与十一腔扩展（纪念碑谷方向）

不新增输入，把已教规则反转语义，制造"惊喜 → 新鲜 → 回看理所当然"。

- [x] `ACollapseGate` 坍缩门：附近井坍缩成黑洞时开启联动门（"喂太多"从惩罚变钥匙）
- [x] `ACheckpointVolume::bInvertContainment` 反向收容场：井只能待在场内，拖出被挡（"场"从围墙变工作台）
- [x] 新增 I 喂饱即钥匙、J 场是工作台、K 双动词终考
- [x] I/J/K 通过无窗口 Python commandlet 直接摆入 `Lvl_CollapsePointDemo`
- [x] 自动测试扩展到 A–K；J 走廊级接收台使抛射解法确定化，A–K 连续 5/5 通过

**里程碑**：同一套动词、同一批白模，靠语义翻面产生"啊哈"，为后续更多"翻面"puzzle 奠定主轴。

## Phase 3.8 — "井是机器"发射动词与十三腔扩展（我们自己的路）

不照搬纪念碑谷视觉悖论，让惊喜从引力物理本身涌现：井是一台会发射质量的机器。

- [x] `ECollapseLaunch` 三态发射：AimThrow（原瞄准丢）/ TangentialSling（切向弹弓）/ RadialBurst（径向喷发）
- [x] `ASingularity::Detonate()`：径向喷发所有环上质量后坍缩
- [x] `ACollapseGate::bDetonateWell` + `DetonateMassThreshold`：关卡级"过载器"，井吃满即强制引爆（与 I/K 的普通质量门互不干扰）
- [x] `USingularityAttractComponent::GetTangentialDirectionAt`：暴露轨道切线方向
- [x] 新增 L 切向弹弓、M 级联喷发；直接摆入 `Lvl_CollapsePointDemo`
- [x] 自动化新增 `ReleaseWellTangential` / `WaitWellDetonated` 动作，扩展到 A–M；L/M 连续 4/4 通过
- [ ] 为切向弹弓接入玩家输入（如中键 / 快速松手手势），当前仅自动化驱动
- [ ] 真正的井→井级联腔（一次喷发喂活下游井）作为 L/M 之后的进阶

**里程碑**：同一口井暴露"发射"新面——切向甩射与过喂喷发，靠物理必然而非视觉错觉制造惊喜。

## Phase 4 — 表现与打磨（Demo 后可选）

- [ ] Niagara 漩涡挂到 `ASingularity`
- [ ] `UCollapsePointCameraFX`：色散 + 径向模糊
- [ ] 反复调：吸引曲线、崩塌力度、破坏阈值
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

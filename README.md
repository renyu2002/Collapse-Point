# 坍缩点 (Collapse Point)

一个没有子弹的射击游戏：你的武器是一颗可以随时创造、又能瞬间关闭的可抛掷微型黑洞。

## 当前状态

**Phase 1 实施中。** 工程位于 [`Game/`](Game/)，已启用官方 `ModelContextProtocol` / `EditorToolset` / `LiveCodingToolset`。

已落地（源码）：`ISuckable`、`APhysicsObject`、`USingularityAttractComponent`、`ASingularity`、`ACollapsePointCharacter`。  
已落地（资产）：`IA_Singularity`、`IA_Nudge`，并写入 `IMC_Default`（RMB / LMB）。

下一步：关闭编辑器后完整编译，再将 `BP_ThirdPersonCharacter` 父类改为 `CollapsePointCharacter`，并在关卡放置物理方块验证。

实现原则：**以 C++ 为准，蓝图最小化**——玩法逻辑、输入、物理与过关条件写在 C++；蓝图仅用于关卡摆放与表现资产引用。详见 [技术规格](docs/TECH_SPEC.md)。

## 文档索引

| 文档 | 内容 |
|------|------|
| [docs/DESIGN.md](docs/DESIGN.md) | 核心机制、物理规则、亮点三问、记忆颗粒 |
| [docs/VERTICAL_SLICE.md](docs/VERTICAL_SLICE.md) | Demo A/B/C 三区内容与验收标准 |
| [docs/TECH_SPEC.md](docs/TECH_SPEC.md) | C++ 类职责、输入、物理公式、调参表、蓝图边界 |
| [docs/ROADMAP.md](docs/ROADMAP.md) | 单人 C++ 优先实现顺序（下一阶段执行） |

## 后续开发顺序（摘要）

1. 新建第三人称 C++ 工程，剥离射击残留  
2. 实现 `ASingularity` 与角色输入（创造 → 吸入 → 爆发）  
3. 可吸入物理物、敌人、触发器与 A→B→C 试验腔关卡  
4. 手感与后处理打磨（预计占开发时间 60%）  
5. 打包独立程序（无菜单、启动即玩）

完整 checklist 见 [docs/ROADMAP.md](docs/ROADMAP.md)。

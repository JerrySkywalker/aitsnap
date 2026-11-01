# AITSNAP-V1-UX-ITERATION-002 — 下一轮开发交接入口

日期：2026-11-01（Asia/Shanghai）。本轮交付是**诊断、概念设计与开发清单**，没有修复产品代码、重打安装包或重新宣称验收通过。概念图为合成示例，尚待 Owner 审计，不是现有软件截图。

## 直接给下一位 AI 的任务

在 `C:\Dev\src\aitsnap` 先核对分支、工作树及 `origin/main`，然后依次阅读本文件、[问题与证据](ISSUES-AND-EVIDENCE.md)、[开发路线和清单](ROADMAP-AND-CHECKLIST.md)、[界面概念及审计点](UI-CONCEPT.md)、[验收矩阵](ACCEPTANCE.md)。本交接已经给出普通技术决策的推荐值；不要重新要求 Owner 复述本次问题。

先完成安装后启动失败及准备过程反馈，再落实“编号＋名称”、人类可读文件命名、独立文字覆盖层、统一图标和可视化工作台。各阶段做针对性验证及正常 PR 审查/合并，保留已有截图、批次、Word 队列、历史配置和验收回执。概念设计尚未获 Owner 批准，先实施明确缺陷和基础模块；完整工作台布局以本概念审计结论为准。

## 当前基线与现场

| 项目 | 已知事实 |
| --- | --- |
| 产品基线 | `b9fad25d20fb2325b584411acc27e7c5e1534601`，对应已合并 PR #22 |
| 仓库 | https://github.com/JerrySkywalker/aitsnap （访问依赖 Owner 的现有授权，不在交接包中放凭据） |
| 文档分支 | `codex/aitsnap-next-iteration-handoff`；不等于产品新版本 |
| 最近产品变更 | PR #21：安装向导、占位比例；PR #22：路径控件首次绘制遮挡修复 |
| 当前 MSI | `C:\Dev\src\aitsnap\out\AITSnap-0.6.2-windows-x64.msi` |
| MSI SHA256 | `89ba6616bb6426d7df4c40e898943aa2a5807192b3a7f00e8954cc5da75dbba6` |
| 安装实效 | 当前用户安装目录的 117 个文件与最终 MSI 清单全部匹配；这只证明部署，不能证明“立即启动” |
| Owner 最新反馈 | 路径框和浏览首次显示正常；安装后“立即启动”失败；显示进度前等待过长 |
| 本机环境 | Win11 Pro Insider 10.0.26340 x64；Qt6.12/MSVCv143；已有 Qt5.15 x64/x86 主机套件 |
| 安装 UI 限制 | Computer Use 明确拒绝 `msiexec.exe`；不得用代理窗口、改名或其他注入绕过。人工反馈和原生日志须分开记录 |
| 外部 HOLD | 实际 Win10、Win7 x64/native x86、Word2010、完整 Owner 视觉/布局及长时资格仍未通过 |

已运行的 AITSnap 是 Owner 的应用，不要为验证随意终止或卸载；本轮没有启动、关闭或修改它。现场进程快照只能说明稍后有进程存在，不能倒推它由安装完成页启动。

## 证据与恢复

- 旧封存：`out/AITSNAP-V1-GOAL-TRAIN-001/WIZARD-062/FINAL-RECEIPT.md`；它早于本次 Owner 反馈，不能继续当作当前问题状态。
- 新诊断：`out/AITSNAP-V1-GOAL-TRAIN-001/UX-ITERATION-002-PLANNING/diagnosis-summary.json` 与 `sanitized-timeline.txt`。
- 本地原始安装日志快照：同目录 `owner-062-install-raw.log`，SHA 和行号已记录。**原始日志包含本机账户/路径信息，保持 ignored，不直接提交或发到外部。**
- 累计预算：`out/AITSNAP-V1-GOAL-TRAIN-001/ledger.json`。原始上限仍为 120 小时，不是本次重新授权的 120 小时。本次结束时的准确消耗/剩余见 `out/AITSNAP-V1-GOAL-TRAIN-001/UX-ITERATION-002-PLANNING/planning-receipt.json`；不要重置历史。
- 开始真实工作前运行 `python tools/goal_train.py start --note "UX-ITERATION-002 ..."`；阶段检查点用 `checkpoint`。阶段/父预算均要遵守。若 D07 原阶段预算不足，先登记新的有界子任务与父预算关系，不得默默重置 D07 已用时。
- 不运行旧 `seal-receipt.py`、`seal-m3-candidate.py` 或 `seal-062.py`；它们绑定历史状态，重跑会覆盖封存信息。

## 边界与工作方式

1. 默认按现有权限和 profile 路由工作；角色名不等于独立只读审查。没有被要求时不自动启动子代理。
2. 使用现有 Qt、VS、vcpkg 和系统安装器；不要为 UI 改造无条件引入 Electron、WebView 或新安装框架。
3. 本轮提出的透明等待状态不授权关闭系统还原、Defender、UAC 或扩大权限；安装慢的事实与 UI 无反馈的事实分开处理。
4. 内部 UUID 保留。所有显示名/文件名改革不能破坏引用、幂等、去重、历史读取、Word job/source 哈希。
5. 用户截图和产品证据不应混入概念图；概念图中的图表、屏幕与数据都是合成示例。
6. 不重复未改变范围的全量 CI；记录准确源码 SHA、包哈希、针对性验证、人工反馈和剩余 HOLD。

## 优先阅读的产品实现

`tools/msi_ui.py`、`tools/build_msi.py`、`src/main.cpp`、`src/main_window.cpp`、`src/desktop/region_editor.cpp`、`src/desktop/scene_editor.cpp`、`src/desktop/scene_layer_item.h`、`src/adapters/capture_files.cpp`、`src/adapters/json_store.cpp`。

通用排障可使用 `diagnose` 技能；界面验证遵守 `computer-use` 技能和实际工具限制。这些建议不要求重做仓库治理配置，不应阻塞当前缺陷修复。

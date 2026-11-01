# 0.8.0 执行记录

Owner 于 2026-11-01 明确授权开始，授予本轮48小时执行上限；嵌套于原120小时累计上限。44小时计划项加4小时收敛预留，未改变其他环境和授权边界。

## W00 — 接纳与契约

- 基线 main/origin/main 同为 `118246e8b8bc1f5949ce3b3ba3a9ae18683761e8`；fetch已执行；无开放PR，只有原工作树及未跟踪的本轮计划，保留后使用 `codex/aitsnap-080-workspace`。
- 实际产品基线仍为0.7.2 `ec82339c78304e67e5d8b3b5937134608ba39972`。本轮未重复安装或操作Owner截图/文档。
- 原账本在 `out/AITSNAP-V1-GOAL-TRAIN-001/ledger.json`，原始快照备份于 `UX080/ledger-before-080.json`；新增 `iterations.080` 记录172800秒上限，新增080-W00–W08各项预算。每次checkpoint同时更新父子用时；恢复不得重置。
- 当前汇总新增 `currentProductStatus` 指向0.7.2回执，旧0.6.2状态保留为历史。缺失Win10/Win7/Word2010和Owner/工具策略验收保持独立HOLD。
- 双绑定保留旧主键字段，新增备用键和每动作启用；动作映射显式化。设置迁移升级版本并备份旧bytes。Word任务保持旧指纹算法可读，新行为冻结于新任务指令。实时缓存不创建证据批次。迷你窗复用既有命令。

后续按Goal记录真实改动、调用与结果；未执行项不预填PASS。同会话diff审阅不冒充独立只读审查。

## W01 — 双热键

三个动作各主/备用两槽，保留旧主字符串，增加每动作启用；settings升级为v5并保存旧bytes。热键注册显式映射动作，同动作规范化重复合并，跨动作重复拒绝；外部冲突恢复旧键，暂停期间配置不重新注册，F12保留键拒绝。输入框聚焦时注销，离开按截图暂停状态恢复；全部入口仍共用既有QAction。

Qt6 Release产品及相关测试编译通过；dual-hotkeys/desktop-adapters/appearance 3/3通过。Qt5 v143 x64宿主dual-hotkeys/appearance 2/2通过。新定向测试验证六槽映射、实际系统注册冲突/回退、pause→configure→resume、关闭后释放、v4迁移备份字节；模拟WM_HOTKEY消息不标为物理键验收。原始日志在ignored `UX080/w01-*`。

初次裸cmake命令命中Strawberry安装，与旧缓存的Qt CMake模块混用导致再生成失败；失败日志保留，随后用既有Qt Tools内确切CMake执行并成功，未重装工具链。编译有局部同名变量warning，后续窄改整理。原生Win7和Owner物理键验收不由本次宿主结果关闭。

W01源 `d4c5b4a60db7b8625b1e3dcf4410360718944cc3` 已经PR40正常合并，merge `315b0fa294549bf485438a7d2ac6255aeb17aa87`。分支规则查询为空，PR预期head及mergeable已核对，未使用admin/force选项。

## W03 — 迷你截图窗

主窗/托盘入口、三动作选择、状态、暂停恢复、可选置顶、位置与动作偏好记忆、主窗返回。共用既有QAction和startCapture；正式捕获前同步隐藏迷你窗，worker在采集前DwmFlush，完成后恢复。单独view偏好文件不改截图证据。补全新增中英文文案，消除前述同名变量warning。

Qt6界面/双热键2/2，Qt5界面1/1通过。两路Win11原生四屏五合成来源验证通过：三动作新采集副作用、序号、无copy-only Word任务；迷你窗覆盖测试来源时，触发后已隐藏，成图指定像素仍为已知红色，完成后恢复；暂停/恢复及返回真实主窗通过。Qt5仅为宿主兼容，不是Win7。

一次测试编译因对未声明Q_OBJECT的子类调用Qt6 findChild模板失败，改为查找QWidget再dynamic_cast；保留失败日志。第一次原生测试与appearance并行占相同全局键，首批超时；改为串行后Qt6/Qt5均通过，未把此并发测试冲突隐藏。原始记录 `UX080/w03-*`。Owner物理输入、全部DPI/断屏视觉验收仍独立HOLD。

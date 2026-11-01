# 0.7.0 交付回执与恢复入口

PROGRAM_ID=AITSNAP-V1-GOAL-TRAIN-001；DISPOSITION=CANDIDATE_DELIVERED_WITH_HOLDS。Owner 最新版本要求 0.7.0 已落实。交付的是完整 Windows 安装候选，软件/组件检查完成；最终可见安装链路及目标机资格未冒充 PASS。原 120h 累计预算不重置。

## 版本与安装文件

- 本轮产品基线：`b9fad25d20fb2325b584411acc27e7c5e1534601`（0.6.2）；原训练 START_SHA：`e6bb5b1ad0650b3a73efcb5e12cba6913bd34b78`。
- 干净打包源码：`69897b90bdda417958f2e04df9910ed1bf8b372a`；`build-provenance.json.sourceStatus` 为空。交付文档合并后的精确 FINAL_MAIN_SHA 由本地最终回执记录，包仍绑定此产品源码，不混称后续文档版本重新构建。
- 推荐入口：`out/aitsnap-0.7.0-installer-69897b9/AITSnap-0.7.0-windows-x64-setup.exe`，18,442,752 bytes，SHA256 `6ff3fe60f09b8e1df6ffb8443fa612c5f95bda90f1ae989636f3a8b04752a7a6`。
- 直接 MSI：同目录 `AITSnap-0.7.0-windows-x64.msi`，18,305,024 bytes，SHA256 `1280e09828019e5555ced07f733a779f5c8e8720a3ae3c30ca3a480af1e2d7db`。
- 离线目录：`out/aitsnap-0.7.0-windows-x64-69897b9/`；运行库、平台插件、中文标准控件翻译、许可证/SBOM、源/依赖/文件哈希记录齐备。主程序、CLI、Word helper、Setup 的 PE 版本均为 0.7.0。
- 此候选未签 Authenticode；未创建公开 Release/tag。Setup 仅校验内嵌 MSI 并启动系统安装器，不提升权限或下载依赖。

原生操作前从托盘退出旧版本。欢迎→路径→准备/进度→完成页保持系统 MSI 流程；完成页可选择“立即启动 AITSnap”。本回执没有宣称新装/升级/修复/卸载和 Finish 可见窗口已实际通过。

## 本轮实现与工作包

| Goal | 源码/组件交付 | 剩余资格 |
| --- | --- | --- |
| N00 | PASS：恢复现场、原 ledger、安装缺陷及历史证据 | 旧安装/真实证据保留 |
| N01 | PARTIAL：文档语法的 MSI File token、显式 --show、快捷方式、重复实例唤回 | Finish 发起→正确进程→可见主窗口；原0.6.2 Owner FAIL不抹除 |
| N02 | PARTIAL：提交先进入准备页、真实 ActionText/进度订阅 | 首帧≤500ms目标、持续重绘、取消、新装/升级/修复实测 |
| N03 | 软件 PASS：稳定编号/名称、备份迁移、UUID引用不变 | 多屏/顶部/小框/高DPI人工审计 |
| N04 | 软件 PASS：可读目录/原图/JPEG/拼图名，独占冲突后缀，UTF8边界 | 真实安装后的体验检查 |
| N05 | 软件 PASS：独立身份文字、主题不改变导出像素；未来图注用冻结名称 | 25–200%人工清晰度；未另加可选栅格输出标题功能 |
| N06 | PARTIAL：单SVG→九尺寸PNG/ICO，四PE和MSI流相同，品牌Setup | Explorer/任务栏/托盘/安装入口原生效果及Owner品牌批准 |
| N07 | 软件 PASS：五个浏览页、默认显示器布局、区域/场景/批次实际浏览 | 最终Owner视觉批准 |
| N08 | 软件 PASS：来源/输出互相高亮、应用保留选择、框选F2改名、旧尺寸提示、历史报告追加 | 框选F2/取消和画布实际交互 |
| N09 | PARTIAL：完整离线MSI/Setup，CAB读回，中文空格路径隐藏启动 | 完整可见安装流程和目标Win10 |
| N10 | 软件/组件 PASS：十组浅深色概念，设置→外观的中/英/系统语言、浅/深/系统配色 | 最终人工批准 |

工作台保留源卡、拓扑、下一次输出和预览；批次页显示磁盘上已保存图片。没有截图时显示占位，旧尺寸提示重新采集。场景独立标签不写入导出；等比适配默认留白，关闭时才拉伸。设置分七类，颜色立即预览并通过应用保存，语言重启生效。

显示器布局仍按配置保存校准，变化后明确要求核对；没有实现多套外接显示器环境自动识别/恢复，不能据本轮 UI 声称已有该能力。旧图片、批次清单、Word作业及其指纹不改写。历史追加读取冻结配置/上下文和源哈希，同批次/同报告已有任务不重复插入。

## 精确验证与证据

主机 Win11 `10.0.26340`，Qt6.12.0/MSVC2022 x64；Qt5.15.2 x64 使用现有 v143 仅作 Win11 host 分支兼容检查。Win7 v142/真实 x86、Win10、Word2010 不由这些检查替代。

- Qt6：appearance（offscreen）PASS、report-queue（fake worker）PASS；其余 core/context/desktop-adapters/scene/context-store/local-host 6/6 PASS。Qt5：appearance/report-queue 2/2 PASS。没有重复无变化的全量 CI，仓库无配置的 Actions 检查。
- 覆盖选择联动、配置原字节备份、编号不复用、浅深色/状态文字对比、标准中文按钮、源/导出像素独立、UTF8长名、旧 job 精确回读、历史追加/重复与篡改拒绝、manifest 字节保留。Word 的当前回归使用 fake worker，不冒充新的真实 Office 排版结果。
- 316条 UI 英文目录与参数替换覆盖 PASS；PowerShell便携启动入口解析 PASS；差异空白检查 PASS。
- 最终完整 MSI：9 dialogs、61 controls，路径、tabcycle、文本/输入框不覆盖、准备页事件、UI-only Finish、静默边界及品牌流 PASS。正常升级身份，无夹具恒假 LaunchCondition。
- 读取最终 MSI 内嵌 CAB 并解压，121个实际文件全部匹配安装回执SHA。Setup仅提取模式在中文/空格路径成功，提取MSI与直接MSI字节相同，未调用安装。
- 最终4个PE实际图标资源均含9帧，逐帧匹配唯一品牌母版派生PNG；MSI Icon/Binary 使用同一ICO。
- 完整包普通路径及中文/空格路径隐藏事件循环启动均 exit0；版本0.7.0、Qt编译/运行6.12.0、64bit、Windows平台及全部Qt/qwindows模块路径来自各自包内。没有调用 --show、发送输入或覆盖原安装。
- `out/AITSNAP-V1-GOAL-TRAIN-001/UX070-INTEGRATION/receipt.json`：源/组件检查、命令日志、10张浅深色组件渲染。组件PNG使用合成夹具，不能冒充原生桌面/Owner验收。
- `out/AITSNAP-V1-GOAL-TRAIN-001/UX070-PACKAGE/`：实际CAB读回、Setup提取、普通/中文路径启动JSON、完整安装回执及文件散列。

源码检查的 C++/资源树为 `0c39032197b6044d6a5fbf47c1ec34564293f5d0`，最终审阅 head `2e8a3c057c98d5851ff6ea2765dad48b929baaf5` 仅追加便携启动脚本/文档，脚本解析另通过；产品合并打包树 `69897b90...` 的 C++ 与测试树相同。PR同会话自审，未宣称独立只读验收。

## 里程碑与剩余阻断

| 里程碑 | 软件交付状态 | 外部/人工资格 |
| --- | --- | --- |
| M1 多屏截图/托盘 | 原有实现保留；本轮适配回归PASS | 最终物理热键/托盘/显示器验收HOLD |
| M2 自由拼图 | 原有渲染与编辑复用；身份/浏览/主题组件PASS | 最终指针交互/缩放清晰度/Owner视觉HOLD |
| M3 Word报告 | 原有现代Word软件闭环历史成立；本轮冻结名称/队列兼容PASS | Word2010、长文档、当前0.7.0人工排版HOLD |
| M4 上下文链路 | 原有实现保留，context/store/host回归PASS | 真实设备执行从未授权/从未运行，Owner验收HOLD |
| M5 全平台验收 | 可体验候选已封装 | Win7/v142、Win10目标、Word2010及人工签收未齐，HOLD |

旧安装日志还原点等待20.236s为历史实测；准备反馈改动尚无新首帧/总耗时数据。Owner原配置启动到托盘=false，所以不能把托盘偏好当作原启动失败的已证实根因。MSI File token 修正依据[微软 MsiLaunchApp 文档](https://learn.microsoft.com/en-us/windows/win32/msi/msilaunchapp-controlevent)；真实失败原因和完成页闭环仍待原生验证。

Computer Use 曾拒绝 msiexec 控制，随后 Owner 用物理 Esc 停止桌面操作。没有代理/重命名/注入/复刻安装向导绕过限制；没有恢复桌面输入。原0.6.2实例和真实配置未终止或改写，只有自建隔离测试 PID25444 被精确核对后清理。

停止原因：源代码、组件与独立包检查已完成；剩余目标依赖已停止的桌面验收、合法目标环境或Owner审计，重复源CI不增加这些证据。WIN7_X64_NATIVE=HOLD；WIN7_X86_NATIVE=HOLD；WORD2010=HOLD；WIN10_NATIVE=HOLD；FINAL_INSTALLER_VISIBLE_FLOW=HOLD；OWNER_VISUAL=HOLD。

恢复：先读本回执、EXECUTION-070.md、原OWNER-HANDOFF和原ledger；核对最终main/产物SHA，不运行旧seal脚本，不复用/安装 TABLE-FIXTURE。只有Owner明确恢复桌面控制后再做受支持的可见操作；安装器仍受工具原限制约束。目标机不可用时保留局部HOLD。

## 累计预算与PR

截至回执准备累计 12.040h，剩余 107.960h / 120h。最终停止数值在原ledger及本地 `UX070-FINAL-RECEIPT.json`，不重置。

本次 0.7.0 产品 PR：#23 外观/身份/命名基础；#24 浏览式工作台；#25 安装器与品牌；#26 联动/历史报告与翻译。完整训练和后续合并记录：

- [PR #4](https://github.com/JerrySkywalker/aitsnap/pull/4) R00: converge requirements and persist cumulative Goal Train accounting；merge `f2d634a45aab2e8613dc026d99978052760818c8`。
- [PR #5](https://github.com/JerrySkywalker/aitsnap/pull/5) R01: verify physical multi-display pixels and offscreen scenes；merge `517f5416f944b4216f7933e9ea8bd6602bf49801`。
- [PR #6](https://github.com/JerrySkywalker/aitsnap/pull/6) D00: introduce Qt-neutral domain and reusable multi-region capture；merge `c91cb680de4e63cac1cd8a274c19cad668695237`。
- [PR #7](https://github.com/JerrySkywalker/aitsnap/pull/7) D01: deliver multi-region capture, configurable hotkeys and tray preview；merge `831ba3895793eb02493560428f7fec5010e084cd`。
- [PR #8](https://github.com/JerrySkywalker/aitsnap/pull/8) D02: add physical multi-display region selection and resize；merge `ae2d294ece7b2b5ef636d2776040acee35824477`。
- [PR #9](https://github.com/JerrySkywalker/aitsnap/pull/9) R02: prove native Word COM save/reopen and define recovery boundaries；merge `f9667da11c8294cd31de724b364e0b53cce6b8cc`。
- [PR #10](https://github.com/JerrySkywalker/aitsnap/pull/10) R03: validate event lineage and versioned catalog references；merge `a3709bf0bc4703b99d5ec329590731f4dc06ee1e`。
- [PR #11](https://github.com/JerrySkywalker/aitsnap/pull/11) R04: admit implementation architecture and pin scene semantics；merge `ac831964756d1ccd73457d0f13050f3ce4f617e9`。
- [PR #12](https://github.com/JerrySkywalker/aitsnap/pull/12) D03: revisioned scene composer, immutable historical replay and M2 preview；merge `750fa6c0cda49c8eee49c3d3c326b8f9aa0835af`。
- [PR #13](https://github.com/JerrySkywalker/aitsnap/pull/13) D04: durable Word queue, isolated STA bridge and marker recovery；merge `3e539b625e36aee41806ebd964b41875b6aed4a3`。
- [PR #14](https://github.com/JerrySkywalker/aitsnap/pull/14) D05: immutable catalog imports and persistent source event association；merge `7b133261e13dd565a745fc677b524385cab91634`。
- [PR #15](https://github.com/JerrySkywalker/aitsnap/pull/15) D06: local capture host, durable CLI intents and event routing；merge `0d1eff1d22ef6ff5d4137fecc56a8233ffcd2ee5`。
- [PR #16](https://github.com/JerrySkywalker/aitsnap/pull/16) L00: preserve current Win7 toolchain and native-access blockers；merge `fd722ea48d99aaca7d9bd23ce957bdc7bfacff61`。
- [PR #17](https://github.com/JerrySkywalker/aitsnap/pull/17) D07: deliver measured offline Win11 preview and honest acceptance matrix；merge `6260114ca000e53c15baf666d56a6f0785ee66d7`。
- [PR #18](https://github.com/JerrySkywalker/aitsnap/pull/18) Record train delivery, L01/L02 blockers and remaining human/native gates；merge `57a410284a5bbe8a6c8094df3cddaa2fab330ade`。
- [PR #19](https://github.com/JerrySkywalker/aitsnap/pull/19) Recover M3 Word jobs and deliver a Windows 10/11 MSI candidate；merge `10d76a4290a7a433f7a9033853d5193cc64406d2`。
- [PR #20](https://github.com/JerrySkywalker/aitsnap/pull/20) Seal M3 0.6.0 installer candidate verification；merge `8feef8b4e1a5f457af55bbd864fcfff159191f37`。
- [PR #21](https://github.com/JerrySkywalker/aitsnap/pull/21) Add 0.6.1 installation wizard and correct scene placeholder proportions；merge `190e77d5594e19ba73c88085932ab9233194b577`。
- [PR #22](https://github.com/JerrySkywalker/aitsnap/pull/22) Fix installer destination controls hidden on first paint (0.6.2)；merge `b9fad25d20fb2325b584411acc27e7c5e1534601`。
- [PR #23](https://github.com/JerrySkywalker/aitsnap/pull/23) Prepare 0.7.0 appearance and readable region identities；merge `e120f58c673476b64c4b966312e7d3b277dec0c2`。
- [PR #24](https://github.com/JerrySkywalker/aitsnap/pull/24) Add the visual workspace and classified appearance settings；merge `3e11ec298226108c62b4808763c8215b8172ab6f`。
- [PR #25](https://github.com/JerrySkywalker/aitsnap/pull/25) Unify 0.7.0 branding and explicit installer launch；merge `a68a00e5f5fb8dc5354f663c51f5c0dfae10596b`。
- [PR #26](https://github.com/JerrySkywalker/aitsnap/pull/26) Complete 0.7.0 workspace linking and historical report integration；merge `69897b90bdda417958f2e04df9910ed1bf8b372a`。

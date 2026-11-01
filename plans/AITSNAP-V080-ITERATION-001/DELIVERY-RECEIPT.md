# AITSnap 0.8.0 候选交付回执

2026-11-02；范围为 Owner 授权的现代版四项功能、针对性验证、审阅合并和安装候选。本轮48h嵌套于原120h，均按实际累计执行计费。候选已交付不等于原M3/M5及全平台人工验收全部完成。

## 交付物与来源

产品源码：`143e95c8591424adda85c2ad489beb7872e4c874`，包内 `build-provenance.json` 记录封装时干净工作树、各源码及DLL来源/哈希。最终文档封存提交晚于产品SHA，不冒充重新构建产品。

交付候选002；候选001保留于 `out/releases/0.8.0-candidate-001`，因为缺源占位图仍显示内部ID而被002替代。规范 `out` 工件已更新为002。

| 工件（相对仓库根） | 字节 | SHA256 |
| --- | ---: | --- |
| `out/AITSnap-0.8.0-windows-x64-setup.exe` | 18590208 | `0791c73834397bfcb8a79f923abdfa27e4b6faad4201c1b1e07da0b9436802fe` |
| `out/AITSnap-0.8.0-windows-x64.msi` | 18452480 | `61295d19d5b21695f39405f326b22aab242b36110cd1269266b0247e7fd869e1` |
| `out/AITSnap-0.8.0-windows-x64.zip` | 18623252 | `b0739494f60c7bffc3217620467ceaf34388dc90698242349ab700ae7eda2d5d` |

主程序SHA256：`c454a9b00c38b8d66620847e475a13327d17b0530eb6eeb3d010509b23edff24`。MSI ProductCode `{0016D2AB-5635-5C79-9AC5-7EBB25E9CDD3}`，PackageCode `{6AF90800-40B5-4C91-B3C3-B4C1AA5CF72E}`。只读安装状态为-1（未安装）；未自动安装0.8.0，未操作Owner真实配置或报告。

## 四项功能及验收对应

| 要求 | 实施位置 | 证明及范围 |
| --- | --- | --- |
| 三动作各两套同时启用的键 | `adapters/hotkeys.*`、`json_store.*`、主窗/设置 | 六槽动作映射、去重/冲突/暂停/回退、配置迁移；实际产品六次系统输入副作用核对；不作为物理按键人工签收 |
| Word自定义文字、可不写文字 | `report_text.*`、`report_text_ui.cpp`、`report_queue.cpp`、`word_bridge_main.cpp` | 三类开关/模板/预览、只图预设、任务冻结、旧指纹兼容；组件与真实设置操作通过，新增helper实际Office执行NOT_RUN |
| 鼠标迷你截图窗 | `mini_capture_window.*`、主窗共享QAction | 模式/位置/置顶记忆、暂停/主窗切回；原生遮挡像素及实际按钮验证；不同目标DPI/全面视觉未签收 |
| 实时来源、拼图、独立图窗 | `live_preview.*`、`capture_visibility.*`、`live_workspace.cpp`、`live_output_window.*`、场景编辑器 | 首次截图前已有来源像素，同帧拼图，暂停/最小化/关窗释放，正式截图隐藏自身；实时缓存不写证据、不增序号、不排Word；目标平台排除能力单列HOLD |

最终缺源修正使用可读图层编号，不显示UUID；保持fail/skip/placeholder策略。不同内部ID的缺源场景生成相同画面与警告的回归通过。

## Goal结果与已合并PR

基线 `118246e8b8bc1f5949ce3b3ba3a9ae18683761e8`。每个PR均同会话diff审阅、定向测试、核对expected head后正常merge；不是独立只读审查。远端状态复核如下，未绕过分支保护，未重复运行无变化Actions。

| Goal / 改动 | PR | merge SHA | 当前结论 |
| --- | --- | --- | --- |
| W00 接纳/契约、W01 双热键 | [40](https://github.com/JerrySkywalker/aitsnap/pull/40) | `315b0fa294549bf485438a7d2ac6255aeb17aa87` | SOURCE/HOST_PASS |
| W03 迷你窗 | [41](https://github.com/JerrySkywalker/aitsnap/pull/41) | `adaaa4b03275debc1b9dac6224c7111ab8a068d6` | SOURCE/HOST_PASS；Owner矩阵未签收 |
| W02 Word文字 | [42](https://github.com/JerrySkywalker/aitsnap/pull/42) | `fdcd406fbc91d1e35358cc37c93f38cd5f1fec52` | SOURCE/组件PASS，真实新版Office PARTIAL/HOLD |
| W04 实时采样 | [43](https://github.com/JerrySkywalker/aitsnap/pull/43) | `5757fe063ec224c4a111f64eec29afb27d095fc7` | SOURCE/HOST_PASS |
| W05 实时工作区/图窗 | [44](https://github.com/JerrySkywalker/aitsnap/pull/44) | `b855c8a859d7e85b799ad585f3994466079a2165` | SOURCE/HOST_PASS |
| W06 集成/版本/性能/手册 | [45](https://github.com/JerrySkywalker/aitsnap/pull/45) | `fd0e98b8e2630c0739b64e8d7dc4db392452d91e` | 源码与自动宿主矩阵PASS；长期/目标验收HOLD |
| W06 缺源可读提示 | [46](https://github.com/JerrySkywalker/aitsnap/pull/46) | `143e95c8591424adda85c2ad489beb7872e4c874` | 两套构建及scene/appearance各2/2 PASS |
| W07 封装 | 产品源码同PR46；最终文档单独封存 | 见上述产品SHA及工件哈希 | CANDIDATE_PASS；实际安装/升级PARTIAL/HOLD |
| W08 验收接续材料 | 本目录平台/Owner清单、路线和本回执 | 文档封存PR | PREPARATION_PASS；未代签任何环境/Owner格 |

完整各Goal实际秒数、累计余额、停止/恢复事件见 ignored `out/AITSNAP-V1-GOAL-TRAIN-001/ledger.json`。执行日志的错误、工具拒绝、首次失败及修正均保留，不清除历史以制造全绿。

## 执行、复用及未执行证明

当前主机为Win11 Insider 10.0.26340 x64；Qt6 6.12.0/msvc2022_64及Qt5 5.15.2/msvc2019_64，实际编译器均v143 14.44.35207，SDK10.0.26100。Qt5检查只代表Win11宿主共享源码。

- W06整合时Qt6、Qt5各14/14 CTest通过；两路原生四屏五合成来源的迷你/实时/三动作/最小化恢复通过，DesktopActivation原生恢复通过。最终PR46只影响缺源提示和语言资源，重建相关目标及两路scene/appearance各2/2；未为文字窄改重复全套CI。
- 实际产品输入验证绑定旧主程序SHA `4c7898a7698e1331de90ff6f615960cf293aa5d99e6ad4f3e224800498374081`。六套键+迷你按钮序号1–7、五个保存批次、两次纯复制、30图片哈希和25原图像素通过；新版包不伪称与该EXE相同，复用仅限未变动作路径。新增缺源变化使用独立回归；002本身有以下实际运行证明。
- 候选002：125项manifest、127项CAB、126项ZIP逐文件哈希匹配；MSI只读检查9个dialog/61个control，PathEdit可见/启用，完成页启动默认勾选；Setup仅`--extract-to`提取，与独立MSI哈希相同。没有调用msiexec。
- 候选002普通及中文空格路径，清除Qt环境、PATH只留Windows/System32后，真实隐藏Windows事件循环启动/退出0，版本0.8.0、四屏、Qt6Core/Gui/Widgets/qwindows均从各自包内加载。此smoke不等于可见主窗/安装完成页启动验收。
- 仅对自建可丢弃中文路径便携副本执行卸载脚本：未改包文件被清除，修改过README及未知sentinel保留。不是MSI升级/卸载证明。Owner安装、配置和报告未被清理。
- 原16Goal/schema/DAG与合同验证通过；短性能采样范围和原始数值见EXECUTION.md。2fps为该合成案例实测，不能外推全GUI/2×4K/24h稳定。

原始证明：`out/AITSNAP-V1-GOAL-TRAIN-001/UX080/w01-*`至`w06-*`、`final-label-*`、`cua-receipt.json`及`performance/`；包层证明：`out/releases/0.8.0-candidate-002/{payload-readback,runtime-receipt,setup-readback,portable-preservation,delivery-manifest}.json`。机器交付汇总在 `out/AITSnap-0.8.0-EXECUTION-RECEIPT.json`，最终实际余额以其中及ledger停止点为准。

## 里程碑与剩余阻断

| 项目 | 软件/宿主现状 | 尚未关闭的门禁 |
| --- | --- | --- |
| M1 多屏截图/托盘 | 既有基础+双热键/迷你新自动证据保留 | Owner物理键、焦点/鼠标/多DPI全面体验OWNER_HOLD |
| M2 拼图与历史原图 | 基础编辑/保存生命周期+实时渲染和像素/原图不变回归 | Owner编辑/视觉签收及实际断屏等HOLD |
| M3 Word追加/恢复 | 旧现代Word16大报告/追加/防重复证据保留其版本；新文字源码与组件通过 | 新文字实际创建/追加/重开/inline-table纯图NOT_RUN/TOOL_POLICY_HOLD；Owner报告视觉与Word2010 ENV_HOLD |
| M4 事件/CLI/IPC | 既有D05/D06源证据及当前组件回归，不扩展命令执行能力 | Owner合成业务流程签收HOLD |
| M5 离线部署/全平台 | 现代候选及包/依赖运行/便携保留证明 | 实际安装升级/完成页启动、Win10、Win7原生x64+x86、Word2010、24h/2×4K与Owner签收HOLD；完整v1 RELEASE_HOLD |

安装器限制原文为 `product policy blocks this app: msiexec.exe`；此前额外Word探针亦被automatic approval拒绝，未创建进程。本轮未换入口/权限绕过。Computer Use后续又报告物理Escape停止，保留该事件；停止后只继续包文件/隐藏启动/文档工作，没有恢复桌面控制。自身排除导致WGC显示下层内容的截图不作本应用视觉证据。

后续使用 [平台与人工验收接续](PLATFORM-AND-OWNER-ACCEPTANCE.md)。未勾选开发清单项保留部分证明的边界，不能读作所有源功能均未实施。外部条件恢复时仍使用原账本；不自动获得新48h/120h，不擅自安装系统/Office或修改权限。

## 停止条件和接续

封存前预算快照（2026-11-01T17:25:20.024845+00:00）：本轮已用9354.542310s（2.5985h），剩45.4015h；父列车已用18.1192h，剩101.8808h。最后文档提交/合并仍计费，最终停止点以机器回执和ledger为准；Escape停止到安全恢复之间不计执行时间。

本轮可安全完成的四项源码、定向验证、合并、现代候选及验收材料已完成；余项均依赖上述实际目标、Owner或工具许可。候选可供Owner手动安装体验，但不是正式全平台发布，也未自动打tag或公开Release。没有为耗尽预算而扩充未授权功能或反复测试。

接续前核对main/远端、包SHA、实际安装版本、目标权限与ledger；仅关闭有实际证据的格子。源码/包层结论与环境验收分列，缺失原生环境时继续保持HOLD。

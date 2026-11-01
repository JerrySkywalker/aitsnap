# AITSnap 0.7.2 安装与修正闭环回执

**0.7.2 已在当前Win11主机完成安装闭环：Owner执行向导并确认完成页启动正常；agent复核安装内容、首次运行现场及同进程托盘隐藏/恢复。Win10/Win7/Word2010资格仍独立HOLD。**

## 已完成

白屏根因：旧版通过Win32 ShowWindow显示托盘隐藏实例的HWND，却没有恢复Qt可见状态，窗口和子控件仍隐藏。原函数原生回归失败，新协议让原实例Qt线程showNormal/raise/activate，并有界确认；两路原生回归通过。真实开发窗口首次绘制和Source Regions实际点击通过；白屏旧实例的UI线程query-status返回OK，支持不是构造/事件循环卡死。

截图动作共用QAction/当前草稿入口：截图保存、截图复制、截图保存并复制都重新采集，保留默认及自定义全局键并显示实际组合；单独的预览复制不重采集/落盘/消耗序号，Ctrl+C只在图片焦点生效。框选编辑区域Ctrl+Shift+R为应用内配置操作。

Qt6/Qt5宿主各8/8定向集成；两路原生4屏5合成来源的按钮副作用/WM_HOTKEY回归通过。0.7.2真实产品还通过Computer Use系统输入F9/F10/F11自定义组合验证三动作序号1/2/3、磁盘批次1/1/2；Both有COPIED回读结果。不是人工/硬件输入验收。377条本地化、源码diff和工具语法检查通过。

完整MSI内嵌CAB125文件/ZIP124文件逐SHA读回；Setup解出MSI与独立包一致；向导实表9页61控件通过；普通/中文空格路径真正Windows平台启动，QtCore/Gui/Widgets/qwindows全部从各包内加载。额外Word包内探针被自动审批blocked by policy拒绝、未创建进程，记NOT_RUN，未替换入口；Word实现源码与0.7.1无差异，旧证明单独保留。

## 候选包

候选源码/干净打包基线：`ec82339c78304e67e5d8b3b5937134608ba39972`（PR37）；产品源与最终编译测试c50fcd8无变化。回执PR只写说明，不替换包来源。

| 文件 | 字节 | SHA256 |
| --- | ---: | --- |
| [AITSnap-0.7.2-windows-x64.msi](C:/Dev/src/aitsnap/out/AITSnap-0.7.2-windows-x64.msi) | 18374656 | `37925786f80dbd9a7730e3886ac95777e1ca87f9029ca8f32aeec8a73ba2b15f` |
| [AITSnap-0.7.2-windows-x64-setup.exe](C:/Dev/src/aitsnap/out/AITSnap-0.7.2-windows-x64-setup.exe) | 18512384 | `d34199f17dbfb25a23b485f95a4743ccc0fd1d24352beda18c2f36afc19a6bd7` |
| [AITSnap-0.7.2-windows-x64.zip](C:/Dev/src/aitsnap/out/AITSnap-0.7.2-windows-x64.zip) | 18545035 | `7251eafa09db4bdb64611584108c6ca8afb5851915b596562620963f326036f4` |

## 实际安装与启动闭环

安装器控制曾被产品策略拒绝，原文`product policy blocks this app: msiexec.exe`。该限制没有被撤销或绕过。Owner按已给步骤退出旧版、手动完成0.7.2向导并保留立即启动，随后明确回复“已安装，完成后窗口正常”。欢迎/路径/安装/完成页点击属于Owner执行，不冒充agent自动点击证明。

在任何主动二次启动前，agent只读核对实际安装目录`C:/Users/jerry/AppData/Local/Programs/AITSnap`：EXE FileVersion/ProductVersion均0.7.2，主EXE SHA`8e26588c37ebbcd1328df0e92b9f223f65eb8b0255e5ecb7a6dae56caea5b192`，并逐项匹配全部125个MSI文件，无差异。原0.7.1 PID54396已退出，新安装目录进程PID56060命令`--show`；Computer Use直接观察该现有窗口的0.7.2标题、完整控件树/画面、三动作及实际默认快捷键，客户区非白屏。

只读Windows Installer API另确认：0.7.2产品状态5（已安装）、VersionString0.7.2，旧0.7.1产品状态-1（未注册），证明升级登记已完成；QtCore/Gui/Widgets/qwindows实际模块均从安装目录加载。最初平铺HKCU Uninstall键未发现产品不作为失败结论，改用Installer官方API留证。

冷启动证据为本次安装完成后新PID的首次打开，由Owner报告与agent现场复核共同支持；没有额外强制结束Owner进程来制造另一次冷启动。随后agent用Alt+F4关闭到托盘，返回窗口清单中主窗消失而PID56060继续存在；正常打开已安装程序后，同PID及HWND24908086恢复，完整UI正常，实际点击工作台成功。原配置/规则相对preinstall快照哈希未变，未触发Owner截图或改写捕获历史。

原生截图与文字证据在`out/releases/0.7.2-candidate-001/installed-validation/`，含finish-observation、tray-restored及receipt.json。完整机器回执`out/AITSnap-0.7.2-EXECUTION-RECEIPT.json`已由pending更新为已安装验证。包仍来自PR37的干净源码，安装文件与已交付包完全一致，无需重复改包或无变化CI。

本轮安装/启动目标完成。保留边界：本机为Win11，不是Win10/Win7；Word2010及额外0.7.2包内Word探针未执行（后者受先前自动审批拒绝，Word实现未变）；不把本轮结果写成原列车M3全目标资格或Owner全面视觉/硬件验收。

## 恢复与预算

本轮源码PR：[35](https://github.com/JerrySkywalker/aitsnap/pull/35)、[36](https://github.com/JerrySkywalker/aitsnap/pull/36)、[37](https://github.com/JerrySkywalker/aitsnap/pull/37)；[38](https://github.com/JerrySkywalker/aitsnap/pull/38)记录先前人工接续边界。均同会话审阅、正常预期head合并，无独立审查/配置Actions声明。完整阶段证据见EXECUTION.md及ignored UX072-AUDIT/ACTIONS/INTEGRATION、out/releases/0.7.2-candidate-001。

原120h累计账未重置：快照累计15.1484h，余104.8516h。此为此前等待人工安装时的历史快照。072-W04已闭环，最终计费以原ledger及机器回执budgetFinal为准；原120h账不续期。

Owner结果已收到并复核。后续从当前main、候选sourceCommit、artifact SHA及原ledger恢复；不重放CI/旧job，不恢复被工具禁止的安装器控制。

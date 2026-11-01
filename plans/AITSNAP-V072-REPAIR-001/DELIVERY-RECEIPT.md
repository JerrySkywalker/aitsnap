# AITSnap 0.7.2 候选与待安装回执

**软件修正和完整包已完成，实际安装尚未完成。最后核对的已安装版本仍为0.7.1；不能宣称0.7.2已安装或Finish全流程已通过。**

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

## 安装阻断和下一步

品牌Setup已实际启动系统安装器，返回窗口AITSnap 0.7.2安装向导（PID35448）。读取该窗口时Computer Use明确拒绝：`product policy blocks this app: msiexec.exe`。没有点击安装，也没有通过CLI安装、代理UI、重命名或提权绕过。最后只读文件核对仍为0.7.1，旧PID54396未被终止，原配置/规则已在preinstall下备份。

已通过异步问题请求Owner从托盘退出旧版，手动完成欢迎→路径→安装→完成，保留“立即启动AITSnap”，并报告结果。此回答尚未收到，不把超时当批准/验收。后续先核对安装目录EXE SHA/版本与包一致、保留原配置哈希，再观察Finish首次窗口；不要先手动二次启动掩盖首次失败。确认后才能继续托盘隐藏/重开、冷启动和已安装动作验证。

工具/原生阻断之外的已授权独立源码、定向验证、版本和离线包路线均已完成。不能把源码绿灯当真实安装验收；Windows10/Win7/Word2010和Owner人工资格仍单独HOLD。

## 恢复与预算

本轮源码PR：[35](https://github.com/JerrySkywalker/aitsnap/pull/35)、[36](https://github.com/JerrySkywalker/aitsnap/pull/36)、[37](https://github.com/JerrySkywalker/aitsnap/pull/37)，均同会话审阅、正常预期head合并，无独立审查/配置Actions声明。完整阶段证据见EXECUTION.md及ignored UX072-AUDIT/ACTIONS/INTEGRATION、out/releases/0.7.2-candidate-001。

原120h累计账未重置：快照累计15.1484h，余104.8516h。当前072-W04待人工安装；最终计费以原ledger为准，等待外部动作时停止本轮计费，不标目标完成。

收到Owner结果后先核对现有worktree/HEAD与原ledger再恢复；不重放已完成CI、不改旧job、不假定安装已完成。不恢复被工具禁止的安装器控制。

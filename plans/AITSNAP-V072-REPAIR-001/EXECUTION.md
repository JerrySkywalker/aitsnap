# 0.7.2 执行计划与恢复记录

Owner 2026-11-01重新授权桌面验证及安装，最终目标改为0.7.2；沿用原120h累计账，不重置预算。main7f24b760c7f4a2ad24daa083221e101f1b269061干净并同步origin。现分支codex/aitsnap-072-repair。Computer Use先前msiexec拒绝不得绕过，新的允许范围以实际工具结果为准。

## 工作清单

- 072-W00（2h）：原生复现、保留安装版本/配置/进程基线、统一动作语义方案。
- 072-W01（6h）：启动白屏根因、失败回归、最小修复、真实托盘/二次启动/首次打开检查。
- 072-W02（6h）：三种新截图动作共享按钮/托盘/全局热键入口，明确复制预览与重新截图区别，旧快捷键迁移保持、无效/冲突提示。
- 072-W03（6h）：Qt6/Qt5定向回归、原生捕获及热键差异验证、0.7.2完整离线打包和包读回。
- 072-W04（6h）：实际安装/启动完整流程、必要修正、正常PR审阅合并、最终已安装版本/回执/预算；原生或工具限制时最小HOLD并继续独立目标。

## 已复现

当前安装0.7.1，EXE SHA22428f1a900622226542d0928e1673568cbe90055826b96b8b00a35fcd3b8ab6，与PR33候选相同。PID54396命令--show，起初无可见主窗；通过已安装EXE再次启动后，同一PID主窗出现全白，辅助树只有QMainWindow和标题栏，没有工作台控件。与Owner现象一致，不是把offscreen结果当原生证据。

排序假设已向Owner说明并自主验证：H1 Win32 ShowWindow恢复了原生窗口而Qt逻辑状态仍隐藏；H2安装环境/构造阻塞应在trace停止；H3逻辑已显示但原生重绘/主题失败。先以原函数的独立原生窗回归建立可见状态反馈，不修改Owner设置/作业。

## 动作统一方案

三种动作均重新采集：截图并保存（原有Ctrl+Alt+S）、截图并复制（原有Ctrl+Alt+C，只复制所选输出，不落盘/不入Word）、截图保存并复制（原有Ctrl+Alt+A）。保留既有自定义热键。按钮、托盘和全局热键共用同一动作与配置快照入口，展示当前快捷键及副作用。另保留“复制当前预览”作为不重新截图的浏览动作，Ctrl+C仅在图片浏览获得焦点时使用；文本输入的Ctrl+C不截获。框选/编辑区域是配置动作，不伪装成截图；历史图片复制不消耗捕获序号。

## 白屏根因及修复证据

原函数原样提取到desktop_activation后，原生Windows回归exit1：底层HWND已可见，但QWidget.isVisible及子控件仍为false。修复为已运行实例接收注册窗口消息，在自身Qt线程中showNormal/raise/activate；发送方使用2秒有界SendMessageTimeout确认协议响应，不用外部ShowWindow跳过Qt状态。找不到尚在初始化的首实例HWND时最多等待2秒。旧版未实现该协议不会被误报为成功激活。

修复后原生回归exit0，反复hide→激活和minimize→激活均恢复Qt可见状态。Qt6完整开发实例（尚未升版，非交付包）PID50948：隔离设置/no old jobs，先Qt隐藏再第二次--show，第二进程exit0；Computer Use实际看到工作台控件并点击源区域成功切页；trace first-paint848ms。原安装PID54396及原设置未改动。不是仅进程创建成功。

两轮诊断夹具结果未算成功：最初第二进程早于首HWND创建，隐藏信息框等待，已核对路径/trace后仅终止自建PID24304；第二轮首进程被Start-Process Hidden启动，Qt逻辑恢复但StartupInfo隐藏了原生首窗，因此不算可视资格。第三轮用Owner已授权的Normal可视测试启动，得到了上述真实画面与交互；所有原始日志保留UX072-AUDIT。自动60s诊断实例自行退出，未按同名终止任何Owner实例。

下一步Qt5原生回归、同会话源码审阅/PR；实际MSI Finish尚未验证。截图动作按前述表统一，不调整既有自定义组合键。

## 统一动作实现与验证

按钮、托盘、WM_HOTKEY路由到同一组QAction和triggerCapture；同一UI草稿先持久化再形成不可变采集快照，CLI事件仍明确使用已应用配置/显式profile选项。三动作旁和托盘显示实际自定义全局组合键，不额外注册一份Qt快捷键；不变组合键不注销重注册，避免每次截图扰动热键。框选/编辑区域使用共用QAction及Ctrl+Shift+R（应用内），明确不触发截图。ImagePreview有“复制当前预览”与仅该图片焦点下的Ctrl+C，复制缓存预览分辨率，不重采集/落盘/消耗序号，文本框Ctrl+C保持。

Qt6/Qt5 appearance+management各2/2最终PASS。新增回归检查托盘共用动作、实际持久化键显示、预览缓存像素复制且设置文件完全不变、空预览禁用。Qt6测试初稿findChild<ImagePreview>未有Q_OBJECT编译失败，改为按名字找QWidget后dynamic_cast；未运行旧二进制充数。组件回归还发现短暂managing scope缓存禁用状态在新建区域后留下按钮失效；动作可用性改为稳定捕获/暂停/配置状态，编辑/管理的暂态由真实dispatch guard和模态保证，修复后再跑通过。

真实Windows平台四显示器、五个红绿合成来源：两路native --capture-contract均exit0。先经过WM_HOTKEY路由（非物理按键证明），再通过真实QPushButton.click调用三按钮分别新采集（非物理鼠标证明）；保存5/复制0、保存0/复制1、保存5/复制1逐项匹配；每次恰好一个sequence，未手工应用的新输出目录生效；复制即使启用报告也无Word job、无图片目录。焦点保持及源像素准确检查PASS。Qt5仍仅Win11/v143宿主，不当Win7资格。

下一步正常PR并冻结动作源码，再统一0.7.2、全套定向集成、完整包与实际安装/首次打开/已有托盘实例验证。原始日志UX072-ACTIONS，377条本地化PASS，不重复无变化CI。

## 0.7.2 实际系统输入验证

PR35 merge78ddc0bfeb49d506ff1235316193f9c1c9daa9ce，PR36 merge de710f66ac43da20fc279c1ec5bd5402f65f4d09。版本集成c50fcd8，二进制PE0.7.2。构建期间的已完成产品目标在独立data-dir验证，不运行仍在编译的测试二进制。

原已安装白屏PID54396的query-status通过真实UI线程返回OK/busy=false；原队列为空，支持不是构造/事件循环挂死，而是Qt可见状态与外部HWND显示分离。原设置未写入。

最终0.7.2产品程序实际显示完整界面及三动作快捷键，Computer Use原生键盘输入Ctrl+Alt+Shift+F9/F10/F11分别触发save/copy/both：序号1/2/3，磁盘batch数1/1/2，无Word任务；Both磁盘清单有COPIED输出，应用代码已做剪贴板读回检查。仅用16×16隔离ROI，不修改Owner配置。这是系统输入注入+原生画面/文件回读，非Owner人工按键或硬件时延证明。

首轮自定义F20已触发保存，但工具拒绝F21（unsupported key），不算copy验证；未换脚本注入被拒绝的F21，改用已支持的普通F9/F10/F11自定义绑定重新验证完整三组。60s诊断实例正常自行退出。用于可视/截图的smoke idle CPU字段超出其hidden/no capture声明，明确不采用这些性能字段；原生输入与batch回执单独保留physical-key-receipt.json。完整安装/Finish仍待最终包，不提前填PASS。

## 最终集成源码检查点

0.7.2两路全部受影响目标构建完成exit0。Qt6/Qt5各8/8定向集成PASS；两路原生activation及四屏5源capture-contract也均exit0，native-qt6/native-qt5保留合成源PNG和batch清单。产品PE、CLI、WordBridge统一0.7.2。377条本地化、Python构建工具语法、diff检查通过。

仅文档在c50fcd8后更新，产品src/CMake/tools/tests无差异；下一步从干净合并源码生成完整离线候选，真实安装完成/首次UI另验收，不因源码绿灯提前标完成。现场0.7.1进程UI线程响应且无队列作业；安装前会保存原设置证据，不改动捕获历史。当前Owner允许桌面/安装，旧的桌面停止文档已更新，工具拒绝仍不得绕过。

## 安装器实际能力边界与人工接续

从完整Setup实际打开0.7.2系统向导后，Computer Use读取msiexec窗口被产品策略拒绝，原文product policy blocks this app: msiexec.exe。已向Owner发出人工退出旧版/完成安装/保留Launch选项的异步问题，尚无回答；最后核对安装EXE仍0.7.1，不能填安装PASS。额外独立Word探针命令被自动审批blocked by policy在进程创建前拒绝，未执行，不写PASS，也未换入口。非UI只读检查和预算记录可继续。实际Win32插件包内smoke已通过，不受该额外探针影响。

候选源ec82339c78304e67e5d8b3b5937134608ba39972，完整包和阶段/阻断/接续见DELIVERY-RECEIPT.md；安装后第一次窗口必须先观察，不能通过主动第二次运行掩盖。下一步唯一依赖是Owner手动安装后的实际证据；没有普通技术决策待问。

## Owner手动安装完成后的原生闭环（2026-11-01）

Owner回复“已安装，完成后窗口正常”。在任何二次启动前核对安装目录EXE版本0.7.2/SHA匹配包，逐项125文件全部匹配。旧PID54396退出，新PID56060 --show现有窗口实际显示完整0.7.2 UI，先保存现场，再做其他输入。

随后Alt+F4主窗从可见窗口清单消失、PID56060仍在；正常打开已安装EXE后同PID/HWND24908086恢复，画面/控件正常、工作台实际点击通过。配置/规则哈希仍与preinstall一致，没有截图或重写Owner历史。安装后的新PID与Owner首次正常报告构成冷启动证据，不声明另做强制退出重启。UI点击向导由Owner执行，agent未控制被禁止的msiexec。

机器回执和DELIVERY-RECEIPT已从pending改为当前Win11已安装闭环；其他系统/旧Word及全面人工资格保持独立HOLD。仅更新文档，候选源码ec82339和已安装包字节不变，不重跑无变化CI。

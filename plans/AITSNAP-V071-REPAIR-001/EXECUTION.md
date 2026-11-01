# 0.7.1 执行与恢复

Owner已批准 REVIEW-AND-PLAN.md，正式启动原120h预算内的071-W00至W07。预算不重置；原ledger为累计账。产品基线7c6a717，审计草案3b21290。桌面停止及msiexec限制保持，真实安装和Win10/Win7/Word2010/Owner验收不能用组件结果代替。

## 首个P0：完整保存路径的释放后使用

新增 scene-lifecycle 回归通过真实 MainWindow 新建按钮打开 SceneEditor，执行保存/取消，再返回并重读已保存设置，使用 offscreen 和独立目录，不发送桌面输入。

修复前首次有选择的保存：accept已经返回，主窗口editScene返回前进程以0xC0000374崩溃。AddressSanitizer明确报告：SceneEditor成员selected_的std::set已析构，随后QDialog销毁子QGraphicsScene/选中LayerItem，selectionChanged仍进入selectionFromCanvas，selected_.clear访问已释放内存。栈定位scene_editor.cpp原181及main_window.cpp原405。不是根据故障模块名称猜测，也不依赖真实截图。

修正：SceneEditor显式析构函数在C++成员仍有效时断开graphics_及layers_到this的选择回调，阻止Qt子对象销毁回调访问已释放的派生成员。未改渲染、源图、队列或历史身份。

回归覆盖12轮新建保存/取消（选中/未选中）、4轮已有场景编辑保存/取消与修订数、2轮有合成实际源图的保存/取消，最终包含主窗销毁。Qt6 Release和ASan修复后exit0；Qt6既有scene检查PASS。Qt5 x64 host同一完整生命周期回归PASS（并非Win7原生资格）。原生日志与此次复现的每条操作关联仍不冒充已取得；安装后白屏没有随此项自动关闭。

原始证据 ignored：out/AITSNAP-V1-GOAL-TRAIN-001/UX071-STABILITY/（before2、asan-before、asan-after、qt6-after、qt5-after日志）。ASan仅使用既有MSVC运行库；独立build/windows-071-asan使用/fsanitize=address /Zi /EHsc。没有系统级PageHeap/注册表诊断策略改动。早期未抑制故障对话的自建测试PID49108经路径核对后终止；未操作未知PID或Owner实例。

下一步：完成Qt5检查、同会话源审阅和正常PR合并此最小修复；继续W02启动取证及其他独立命令/界面/报告目标。W03至W07尚未实现，0.7.1完整包尚未生成。禁止把现有0.7.0包当新版本交付。

## PR28已合并，进入管理能力

首项源码修正PR28：https://github.com/JerrySkywalker/aitsnap/pull/28 ，源码a7d020b3b7673ab6298837b97feab8c3b337defa，merge b5dc7feb78b2aa5d614599f68cc9a284e6d93e99。Qt5完整生命周期PASS。版本尚为0.7.0，待0.7.1功能集成统一升版后打包；没有新的可安装包，原安装未改动。

当前分支codex/aitsnap-071-management，基于上述合并。071-W00为PARTIAL（保存基线PASS、实际启动链未闭合），W01为软件PASS，W02为PARTIAL（原生限制保留；仍可源调查），W03 ACTIVE。下一步首先分离“配置可编辑保存”与“捕获可执行”的校验：core/domain.cpp validateProfile当前拒绝空源/全部停用，json_store.cpp:67及main_window.cpp:175使用同一校验；collectSettings还对regions.front取样。应允许配置空态同时保持CaptureService的实际捕获约束，再实现按稳定ID删除/引用清理/统一命令，最后接右键和批量多选。

原始ASan栈以及失败/修复后结果均在out/.../UX071-STABILITY/，receipt.json绑定源码。独立ASan构建和Qt6/Qt5构建已完成，无构建任务悬挂；有一个早期故障诊断时ExecutablePath不可读的PID19300，未触碰，不能按同名批量结束。不要重复无变化全量CI。

## W03未提交源码检查点

当前codex/aitsnap-071-management基线b5dc7feb，工作树有本轮授权改动，禁止reset/clean。已新增validateProfileDefinition，允许空区域/全部停用配置保存；CaptureService仍使用严格validateProfile，MainWindow在入队/消耗序号前也拒绝无启用来源。修复collectSettings对regions.front的空访问，空态清除旧预览、禁用截图动作，重新添加区域后恢复。Qt6 core/appearance空态持久化和程序化拒绝回归PASS。

新增adapters/settings_commands.{h,cpp}纯候选操作（尚未绑定UI/落盘）：按profile范围删源、全局删场景及清理输出/复制引用、停用受影响规则但保留诊断引用、按身份重定位activeProfile、删光配置后新建空白容器。settings_command_tests覆盖跨配置同源ID不误删、场景引用清理、规则停用、活跃配置和空态持久化，Qt6 PASS。

host_controller补充停用规则允许保留已删除对象的引用，启用规则仍严格校验；规则编辑器遇到失效profile显示缺失条目而不悄悄切到“当前配置”。这些更改已编译，实际UI/规则集成回归随菜单接入补齐。localization检查322条通过，无DEBUG071残留；不把它们当已实现菜单。

下一步必须继续：MainWindow统一对象命令、确认前依赖摘要、配置/规则持久化失败恢复、按钮/右键/多选/快捷键共用入口、配置管理页与规则批删，再做真实命令UI集成回归和Qt5分支检查，然后正常PR审阅合并。纯命令当前没有调用者，尚不算用户能力交付。W02源调查和原生HOLD、W05三列Panel、W06默认Word、W07升版打包仍在完整目标中，未实现。

本地UX071-MANAGEMENT下保存working-tree.patch及新文件副本用于恢复。最新Qt6 appearance重新编译已完成，无活跃构建句柄；尚未对最后host_controller改动重新运行appearance，用下一轮UI集成新增覆盖验证，不重复旧全量CI。原安装保持未动。

## W03/W04 管理能力集成完成（组件验证）

已绑定统一对象命令到源/场景工具栏、源卡/工作台卡/拓扑/输出菜单、配置管理器，支持单项/多项删除、空白新建与明确复制、重命名、启停/参与输出。图层菜单复用原模型删除与Undo；规则编辑器增加新建/复制/多删，失效引用保持停用且可见。列表多选与勾选独立，刷新保留选择，菜单展开期间冻结重建；空白右键不误删当前项。历史批次/图片/报告菜单复用打开、重排、追加、复制和队列核对，不实现物理证据删除。

删区域仅改变所属profile；保留其他profile同ID源和全局模板，取消受影响输出并停用所属/动态配置的坏规则。删场景清理所有profile输出/复制引用，停用依赖规则；删除全部配置生成一个显式空容器。collectSettings/profileFromJson允许空态；真实捕获仍在入队、消耗序号前拒绝。配置/规则写入先暂存并保留旧配置，规则提交失败回滚；恢复文件尚在时重启和应用均保留HOLD，不静默覆盖。此为常规写入失败恢复，不声称多文件断电原子事务。

批量删除使用普通QDialog，含可滚动人类名称列表、引用影响和默认取消。最初Qt5 offscreen在QMessageBox::showEvent空指针崩溃，ASan与官方5.15.2源码定位Windows辅助函数未检查platformNativeInterface返回值；没有归因为Owner的Qt6启动白屏，也没有跳过Qt5确认路径。改造后同一实际确认按钮流程Release/ASan均exit0。官方源码：https://raw.githubusercontent.com/qt/qtbase/v5.15.2/src/widgets/dialogs/qmessagebox.cpp 。

Qt6与Qt5的core/appearance/management/scene/settings-commands各5项全PASS；管理完整动作链两路ASan exit0。覆盖确认/取消精确文件字节、规则路径写入故障无半成功UI、跨profile引用、全删空态/编号高水位、复制新身份、规则失效重启/取消不写、遗留恢复文件保持原状，以及真实offscreen菜单在palette刷新时保持空选择。localization 362条PASS，diff检查通过。上述是offscreen组件和Win11宿主Qt5检查，不能代替真实Win7、原生键鼠或Owner视觉资格。

原始证据：out/AITSNAP-V1-GOAL-TRAIN-001/UX071-MANAGEMENT/{qt6,qt5}-final-tests.log及*-final-asan.{stdout,stderr}.log。源码仍未升版，完整0.7.1安装包未生成。下一步W05统一面板/关联与选中样式，再W06默认按日报告和W02启动源调查；最终W07版本集成、离线包与回执。桌面停止及msiexec限制保持。

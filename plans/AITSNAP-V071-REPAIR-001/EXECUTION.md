# 0.7.1 执行与恢复

Owner已批准 REVIEW-AND-PLAN.md，正式启动原120h预算内的071-W00至W07。预算不重置；原ledger为累计账。产品基线7c6a717，审计草案3b21290。桌面停止及msiexec限制保持，真实安装和Win10/Win7/Word2010/Owner验收不能用组件结果代替。

## 首个P0：完整保存路径的释放后使用

新增 scene-lifecycle 回归通过真实 MainWindow 新建按钮打开 SceneEditor，执行保存/取消，再返回并重读已保存设置，使用 offscreen 和独立目录，不发送桌面输入。

修复前首次有选择的保存：accept已经返回，主窗口editScene返回前进程以0xC0000374崩溃。AddressSanitizer明确报告：SceneEditor成员selected_的std::set已析构，随后QDialog销毁子QGraphicsScene/选中LayerItem，selectionChanged仍进入selectionFromCanvas，selected_.clear访问已释放内存。栈定位scene_editor.cpp原181及main_window.cpp原405。不是根据故障模块名称猜测，也不依赖真实截图。

修正：SceneEditor显式析构函数在C++成员仍有效时断开graphics_及layers_到this的选择回调，阻止Qt子对象销毁回调访问已释放的派生成员。未改渲染、源图、队列或历史身份。

回归覆盖12轮新建保存/取消（选中/未选中）、4轮已有场景编辑保存/取消与修订数、2轮有合成实际源图的保存/取消，最终包含主窗销毁。Qt6 Release和ASan修复后exit0；Qt6既有scene检查PASS。Qt5 x64 host同一完整生命周期回归PASS（并非Win7原生资格）。原生日志与此次复现的每条操作关联仍不冒充已取得；安装后白屏没有随此项自动关闭。

原始证据 ignored：out/AITSNAP-V1-GOAL-TRAIN-001/UX071-STABILITY/（before2、asan-before、asan-after、qt6-after、qt5-after日志）。ASan仅使用既有MSVC运行库；独立build/windows-071-asan使用/fsanitize=address /Zi /EHsc。没有系统级PageHeap/注册表诊断策略改动。早期未抑制故障对话的自建测试PID49108经路径核对后终止；未操作未知PID或Owner实例。

下一步：完成Qt5检查、同会话源审阅和正常PR合并此最小修复；继续W02启动取证及其他独立命令/界面/报告目标。W03至W07尚未实现，0.7.1完整包尚未生成。禁止把现有0.7.0包当新版本交付。

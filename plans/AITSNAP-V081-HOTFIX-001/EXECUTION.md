# 0.8.1：Win32 1409 阻断编辑入口修正

日期：2026-11-02。Owner反馈0.8.0无法进入具体编辑界面，状态显示“动作1的快捷键注册失败，Win32 1409”。基线为main/origin/main `dd9ae87948ed7ae08eb4fe69b002dbe71cbd7108`，工作树接纳时干净，使用`codex/aitsnap-081-hotkey-unblock`。继续原48h/120h剩余额度，账本增加`080-H01`，没有重置预算。

## 复现与原因

在实际`MainWindow`调用点建立失败回归：先用系统`RegisterHotKey`占用保存键Ctrl+Alt+Shift+F20，再构造窗口并单击真实`newScene`按钮。修复前`scene-lifecycle`在0.27s内失败：`occupied saved shortcut must not block scene editor entry`。原始构建与失败记录保留在ignored `out/AITSNAP-V1-GOAL-TRAIN-001/UX081/repro-*`。

Windows [错误1409](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--1300-1699-)是热键已注册；[RegisterHotKey](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey)通常在按键被另一注册占用时失败。无法编辑的程序原因已确认：区域/拼图/迷你/鼠标截图共用`applySettings()`，它把热键注册失败直接当成配置应用失败，入口随后返回。

排查过的其他假设：另一AITSnap实例或其他程序占键有可能，现场只读观察到两个相同安装路径的进程，不能证明具体占用者。磁盘安装程序版本0.8.0，不能仅凭磁盘版本推断两个已加载进程的实际版本。没有结束它们、修改Owner配置或删除锁文件。录入/恢复路径另有状态缺陷：失败恢复保留内部暂停，导致后续配置静默不注册；启动失败后还未保留待恢复键，可能恢复空集合并误报成功。

## 修正行为

- 已保存按键未改变时，外部可用性失败记警告，允许保存其他设置、进入编辑器、迷你窗及鼠标截图。
- 用户主动改键时仍执行语法/重复/系统冲突验证；失败拒绝该次更改，保留原配置与旧键回退。明确用户暂停仍保留。
- 恢复请求和注册成功分离；注册失败不会把鼠标截图强制重新暂停。启动先保留期望绑定，再尝试注册，暂停/恢复不会误用空集合。
- 错误包含可读动作名称、实际按键、Win32码及换键/关闭动作热键的处理方法。英文目录补齐。
- 版本提升0.8.1，配置仍为schema5；便携入口沿用0.8数据根，补丁升级不另开空配置。

## 验证范围

新增/增强回归位于`scene_lifecycle_tests.cpp`（实际编辑入口、无关设置保存、启动冲突恢复）、`appearance_tests.cpp`（运行中暂停后被占键、恢复失败仍能鼠标截图、释放后重注册、新改键冲突仍不提交）、`hotkey_tests.cpp`（明确按键/1409、恢复失败后改键仍注册）及`desktop_tests.cpp --hotkey-conflict`（持续占键下原生鼠标三动作、迷你窗、区域编辑器）。

最终构建、执行结果、PR/产品SHA及包哈希由后续交付回执记录，不提前填PASS。Qt5在Win11宿主的结果不代表Win7；实际安装器点击、新版Word、Win10/Win7/Word2010及Owner人工视觉保留既有HOLD。不绕过msiexec或Word的先前策略拒绝。


## 当前实测检查点

Qt6与Qt5宿主的dual-hotkeys均退出0；新增失败恢复后改键及具体按键/1409诊断断言通过。Qt5宿主appearance、scene-lifecycle各通过（2/2），包括修复前失败的同一编辑入口、非热键设置持久化、暂停期间被占键后恢复、释放占用后重注册及主动改键冲突不提交。Qt6对应界面/原生及候选封装待后续结果，不预填。


Qt6 appearance已通过（1/1），包含恢复失败不强制鼠标暂停及释放后重注册。首次Qt6原生测试退出2，明确记录`NATIVE_HOLD SESSION_LOCKED Windows session is locked; capture was not performed`，没有进行真实截图/区域/迷你验证；前置adapter测试通过不能替代这些原生项目。已异步请求Owner解锁；继续构建与包校验，不绕过锁屏。


Qt6最后的lifecycle运行先在“恢复冲突仍提示1409”组合断言失败。构建日志显示该测试的MainWindow对象来自较早进行中的编译；源文件UTC00:32:22、对象00:33:03，而产品对象在后续00:50:46已重编。保留失败，不改产品逻辑；只删除已核对位于out/build内的该测试main_window.obj并重编，将同一组合断言拆成三个等价断言后1/1通过（2.12s）。没有放宽测试或将原生HOLD当通过。最终产品MainWindow对象在源码冻结后重新生成。Owner随后确认解锁，补跑原生验证。


解锁后原生首次补跑在当前2显示器/5来源上通过精确像素、三种鼠标截图副作用和迷你窗；区域编辑取消及遮罩存在亦通过，随后旧测试断言“编辑期间切到另一配置”失败。基线dd9ae87的objectAction已有busy/editingRegions/editingScene/managingObjects保护，并不是本轮新加限制。保留这次失败，修正测试为更严格的当前合同：编辑期间切换被拒绝、接受后只提交原配置、编辑结束再切换成功且两份配置区域均未覆盖；产品保护没有删除或放宽。最终原生重跑结果另记。


## 源码验证完成

最终Qt6原生补跑退出0：当前2显示器、5个已知16×16合成来源，源像素/剪贴板精确、五图保存、三动作副作用、焦点、迷你隐藏恢复/暂停、区域编辑取消/捕获保护/接受、编辑期间拒绝配置切换及结束后切换均通过，全程保持保存键被占用。日志`UX081/native-final.log`与首批batch.json保留；不是物理按键或人工视觉签收。Qt6与Qt5热键组件退出0；Qt6 appearance和fresh lifecycle各1/1，Qt5 appearance/lifecycle2/2。Qt5末次组合断言后来仅拆成三个等价诊断断言，已有条件证明沿用；native测试旧切换断言修正只在Qt6原生执行。全部产品源码已冻结，不重复未变化CI/性能/Word测试。英文目录450项覆盖通过，diff检查通过。

后续从正常合并后的干净SHA封装0.8.1；安装器实际点击仍受现有策略限制，不将包表/隐藏启动记成安装验收。

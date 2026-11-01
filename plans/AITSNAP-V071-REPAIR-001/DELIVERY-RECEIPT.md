# AITSnap 0.7.1 完整执行与交付回执

**结论：完整离线安装候选已生成；安装完成首次启动白屏仍为根因 UNPROVEN/原生 HOLD。0.7.1 全验收和 M3 整体资格尚未完成。**

候选源码 `080d62e66b41e918e20c4a68bfc2f9d612b776ad`（PR33 merge），干净工作树打包。最终编译/8项测试绑定f50172c4e1b349ad5ca68d150aba9624a43feb99产品源；至候选merge仅说明/合并提交，`git diff ... -- src CMakeLists.txt`为空。包内build-provenance逐文件记录源码及DLL来源SHA，sourceStatus为空。后续回执PR只更改文档，不能把包来源改写为该文档merge。

## 交付文件

| 文件 | 字节 | SHA256 |
| --- | ---: | --- |
| [AITSnap-0.7.1-windows-x64.msi](C:/Dev/src/aitsnap/out/AITSnap-0.7.1-windows-x64.msi) | 18366464 | `29ade6eca569a54db6ecea0e140ec469965a54cd70b48114c177e77bb0522f57` |
| [AITSnap-0.7.1-windows-x64-setup.exe](C:/Dev/src/aitsnap/out/AITSnap-0.7.1-windows-x64-setup.exe) | 18504192 | `fa70152330783dd67f182c1523ae9250594a6ab3f7d7030d3b3409f1c25cbee3` |
| [AITSnap-0.7.1-windows-x64.zip](C:/Dev/src/aitsnap/out/AITSnap-0.7.1-windows-x64.zip) | 18540273 | `6b96742d30e8680b0fd72026b8fd80cbafbf9bd4b3e021bc5cd97f454bdec353` |

安装说明：`C:/Dev/src/aitsnap/out/AITSnap-0.7.1-INSTALL.zh-CN.md`；机器可读完整回执：`C:/Dev/src/aitsnap/out/AITSnap-0.7.1-EXECUTION-RECEIPT.json`。Setup包含与独立MSI相同字节，ZIP为完整便携包。未发布GitHub Release、未改动现有安装。

## 本轮合并PR

| PR | 内容 | merge SHA |
| --- | --- | --- |
| [#28](https://github.com/JerrySkywalker/aitsnap/pull/28) | 保存场景析构释放后使用 | `b5dc7feb78b2aa5d614599f68cc9a284e6d93e99` |
| [#29](https://github.com/JerrySkywalker/aitsnap/pull/29) | 对象管理/批量操作/右键 | `cf14e7b7530b4f7e9fd02d24ac9dca0497b2f03b` |
| [#30](https://github.com/JerrySkywalker/aitsnap/pull/30) | 工作台统一Panel与关联样式 | `b346a3e38436d64a114cb0f4228ba0ba736160ab` |
| [#31](https://github.com/JerrySkywalker/aitsnap/pull/31) | 默认按日Word与手动迁移 | `4b4d7a2da741f7269b5e59ed987ffd46ecfcdf1d` |
| [#32](https://github.com/JerrySkywalker/aitsnap/pull/32) | 可选启动阶段诊断，白屏HOLD | `3c52c830fe755e29acca8a106e686dba82c04d31` |
| [#33](https://github.com/JerrySkywalker/aitsnap/pull/33) | 0.7.1版本集成/动态规则修正 | `080d62e66b41e918e20c4a68bfc2f9d612b776ad` |

PR均正常合并并匹配预期head。审阅为同会话源审阅，无独立审查声明；未配置GitHub Actions checks，没有重复无变化CI。早期列车PR和研究/开发明细保留在[原列车回执](../AITSNAP-V1-GOAL-TRAIN-001/EXECUTION-RECEIPT.md)，本回执不改写历史证明。

## 里程碑与验证层

| 项 | 当前结果 | 证据/限制 |
| --- | --- | --- |
| W00复现基线 | PARTIAL | 原图/崩溃审计、保存路径基线已留证；真实安装启动时间线缺失 |
| W01保存崩溃 | 软件PASS | 修复前普通崩溃+ASan UAF，修复后Qt6/Qt5完整MainWindow生命周期与ASan通过 |
| W02启动白屏 | PARTIAL / UNPROVEN | 文档参数核对、启动分段trace和隔离配置smoke；真实Finish/首帧未取得，不宣称已修复 |
| W03对象命令 | 软件PASS | 空白新建/复制、单删多删、引用清理、写入失败/恢复、跨配置作用域、动态规则回归 |
| W04右键/批量 | 组件PASS / 原生HOLD | 按钮/菜单共享动作、真实offscreen弹出菜单测试；物理键鼠/Shift+F10未验收 |
| W05面板/主题 | 组件PASS / 原生HOLD | 同几何Panel、区分选中/关联、浅深图查看及像素不变；混合DPI/Owner视觉HOLD |
| W06默认Word | 软件及本机Word16合成PASS | 自动/指定、迁移、首份未生成、重启/跨日/目录切换/冲突/移动、冻结目标与FIFO |
| W07集成打包 | 候选PASS / 整体PARTIAL | 完整MSI/Setup/ZIP，PE均0.7.1；真实新装/升级/卸载/Finish及目标机缺证 |
| 原列车M3/系统资格 | HOLD | D07仍PARTIAL；Win10无目标，Win7/Word2010及Owner人工验收未被源码绿灯替代 |

最终定向测试：Qt6与Qt5宿主各8/8（core/context/appearance/scene-lifecycle/management/scene/report-queue/settings-commands）；localization369及构建脚本语法/diff检查通过。Qt5是Win11/v143宿主检查，不能当Win7二进制资格。

包验证：122个package manifest文件逐SHA核对；直接读取MSI内嵌CAB并展开124个文件逐SHA匹配；向导实表9个对话框/61个控件通过；Setup仅执行--extract-to，提取MSI哈希与原包一致；ZIP回读123个文件通过。没有调用msiexec安装。

普通路径与中文空格副本的隔离offscreen事件循环退出0，Qt Core/Gui/Widgets实际加载路径位于各自包目录。**offscreen平台插件来自SDK测试路径；随包qwindows只做了字节校验，未取得原生首帧。** 此项不能证明双击安装、窗口可交互或目标OS兼容。

实际包内WordBridge从中文空格路径运行：Word16.0新建→队列对象重启→同目标追加，两个job COMMITTED；DOCX回读2个drawing/2个COMMITTED标记，源图SHA未变。原始记录在out/releases/0.7.1-candidate-001/packaged-word及packaged-word-readback.json。只使用独立合成文档，不是人工排版/Word2010验收。

## 原始证据与恢复

`out/AITSNAP-V1-GOAL-TRAIN-001/UX071-{AUDIT,STABILITY,MANAGEMENT,PANELS,REPORT,STARTUP}/`保留失败、诊断、修复后日志。误跑旧二进制的首轮Panel图/较早lifecycle记录已明确排除；最终日志均在相应final路径。交付原始证据集中在`out/releases/0.7.1-candidate-001/`，包括installer receipt、CAB读回、模块加载smoke、版本、组件图、Word和artifact回执。所有out资料为本机ignored；tracked计划与回执为可恢复入口。

如会话中断，先检查git worktree/status及origin/main、此回执的候选sourceCommit、artifact SHA和原ledger。不要重新安装、重放Word job、覆盖旧配置或renew预算。没有悬挂构建或测试任务；没有按进程名称关闭未知Owner实例。

## 预算与剩余阻断

原累计硬上限120h未重置。ledger快照 `2026-11-01T09:50:51.357451+00:00`：累计14.1133h，余105.8867h；所有叶目标均未触达上限。最终停止时的精确秒值以`out/AITSNAP-V1-GOAL-TRAIN-001/ledger.json`为准，不用旧回执时间覆盖当前账。

剩余分支逐项审计：W01/W03/W06无待做源码修正；W04剩物理交互；W05剩原生DPI/Owner视觉；W00/W02剩真实安装启动复现和首帧；W07剩真实事务及目标机验收。可独立的源码、组件、文档、完整候选、CAB/ZIP/Setup/包内Word路线已推进至上述结果。没有证据支持继续猜测性修改白屏代码，也没有其他尚未执行的已批准独立目标。

同一原生限制已在连续执行检查点反复出现，当前才在独立路线耗尽后停在HOLD：Owner已停止桌面输入；Computer Use此前拒绝控制msiexec.exe。没有通过脚本、代理窗口或提权绕过。Win10无已授权目标、Win7/Word2010环境缺失；不安装依赖/改ACL/启用Hyper-V来伪造资格。

恢复所需是允许范围内的新原生证据/目标，而非普通技术决策。原预算仍有余额，停止原因是剩余验收分支没有可安全执行的路径。

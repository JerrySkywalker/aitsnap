# 0.8.2：整窗黑屏的显示兼容修正

2026-11-02，Owner报告已安装0.8.1无法使用，整窗黑色/无内容。接纳main/origin/main `40e5441594458989f9fe78ec64f160c5b407ef38`干净树，使用`codex/aitsnap-082-startup-repair`；在原48h/120h余额内追加080-H02，未重置预算。

## 复现、证据与归因

安装文件0.8.1的主EXE哈希与已交付包一致；首次检查未有存活进程。隔离启动记录首次paint派发374ms、事件循环424ms、进程有响应，UIA有完整工作台控件；Owner确认该窗口仍整片黑色。因此这些软件迹象不能证明实际画面可用。

Owner确认本机屏幕直接观察。新建同一进程、同一Qt/Fusion样式的固定蓝绿对照窗：A普通（affinity=0），B仅增加Windows窗口排除（affinity=17，API成功）。Owner确认A正常、B黑色/无内容。确认根因是自动窗口排除与当前实际显示路径不兼容；不是“没有启动”。系统有Intel Arc B390实体输出及未接入桌面的GameViewer虚拟设备；未隔离到具体驱动，不声称GameViewer或Intel单独负责。[Microsoft API说明](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity)解释了其内容显示限制。诊断没有解除既有窗口的排除来获取图像，未保存任何被排除窗口的截图。

原生回归在0.8.1既有adapter库上失败：`Default compatibility display must not enable content exclusion`，证明默认排除仍被启用。最初链接LNK1104源于自建限时对照探针占用测试EXE；关闭记录PID对应探针后重试，失败记录保留，未结束Owner进程。

## 配置来源纠正

诊断中发现Microsoft Store Python对AppData重定向：其同名路径读到私有副本schema5/6321字节，原生文件操作读到实际配置schema4/5227字节。第一份隔离副本因此不作为Owner配置证据；整窗黑屏仍有Owner独立确认。已用原生PowerShell Copy-Item将实际配置复制到ignored out，再在隔离副本关闭Word、重定向输出目录。原生SHA核对原配置未改写。原始私有配置和其哈希不提交仓库。

[Python官方限制说明](https://docs.python.org/3.13/using/windows.html#redirection-of-local-data-registry-and-temporary-paths)与本次两个文件视图的对照一致；后续真实AppData检查只用原生文件访问，不用Store Python视图冒充。

## 修正与保留行为

- 新默认兼容显示，不自动给窗口设置内容排除；原配置缺失appearance.compatibleDisplay时按true读取。schema5保持，新增可选布尔项，语法不合法仍拒绝。
- 正式截图隐藏主窗、迷你窗和实时图窗，在worker采集前DwmFlush，完成后恢复主窗原窗口状态与可见性，不主动抢前台。
- 实时预览的自身遮挡采用已有逐来源阻断，显示移开窗口提示；其他来源继续，不能把自身界面或黑块当正确来源。旧排除模式作为明确可选项保留，设置中注明下次启动及驱动风险，不伪称当前机器该高级模式视觉合格。
- 外观新增兼容选项；`--compatible-display`为本次启动覆盖，不自动改写保存的偏好。启动全程使用新窗口，未在既有受保护窗口上关闭属性进行工具检查。
- 保留0.8.1热键冲突恢复、三动作、Word配置、迷你和实时服务；不修改系统/驱动/权限，也不删除或重建Owner区域、拼图和报告。

## 验证检查点

源码/测试冻结后两路构建完成：Qt6 6.12.0、Qt5 5.15.2/v143 Win11宿主。appearance各1/1通过，含旧配置默认兼容、设置往返、启动覆盖不改写原文件，以及既有热键和外观回归。453条英文目录覆盖通过。

真实0.8.2产品已使用原生复制的实际旧配置隔离启动，首次paint375ms、事件循环412ms，UIA识别版本0.8.2、进程有响应；等待Owner对本机界面及选项卡的实际确认，不凭这些迹象提前写视觉PASS。原生兼容截图/主窗隐藏恢复、产品封装及后续结果由交付回执补记。Win10/Win7/Word2010、安装器与完整M1–M5签收保持原有独立HOLD。

证据根为ignored `out/AITSNAP-V1-GOAL-TRAIN-001/UX082`。前述旧AppData来源已标INVALID，原日志与失败不覆盖；后续只按真实执行结果关闭条目。


## Owner显示确认与截图精确性

Owner已确认初版0.8.2隔离产品界面正常且选项卡可切换；此确认只关闭本机整窗黑屏，不代签其他M1–M5。随后原生兼容模式测试发现主窗hide后存在DWM退场动画残影，首来源像素为#d90000而不是#ff0000。原失败日志保留，像素预期未放宽。兼容模式对AITSnap自身窗口设置DWMWA_TRANSITIONS_FORCEDISABLED，再隐藏和DwmFlush；不修改系统全局动画/驱动/权限。

相同命令重跑退出0（native-transition.log）：双屏5合成来源，保存键持续占用，主窗真实可见且覆盖r0；确认主窗affinity0、实时遮挡仅报对应来源、捕获前隐藏、完成恢复且不抢前台，五图原始像素、三动作/剪贴板/序号、迷你和区域编辑完整通过。单次首捕获206.517ms，仅此次案例耗时，不当完整性能基准。

原Owner视觉确认绑定动画修正前的EXE；后续最终二进制的显示/包验证单独记录，不伪称两份EXE相同。正在重链最终产品及两套相关回归。


最终产品已重链，Qt6/Qt5默认显示属性及重复托盘恢复原生测试均通过（activation-qt6-final.log、activation-qt5-final.log）。最终产品默认兼容窗口已重新打开、UIA识别0.8.2并置前，EXE身份记录在final-default-process.json；等待该份二进制的Owner确认。最终包校验会逐字节匹配此EXE。两路appearance未因仅native路径的动画调整而重复：其offscreen路径不调用DWM，原通过证据范围明确沿用。


## Owner追加显示器修正及打包硬门

Owner要求正常布局只显示当前实际活动屏幕，热插拔/位置/分辨率变化实时刷新，记忆显示器组合与截图方案绑定。现有DisplayCanvas将current与calibrated历史显示器合并绘制，导致当前2屏仍显示4屏；本轮继续修复，黑屏改动保留。

**Owner明确要求：最终版本截图功能测试完成且通过后才允许打包。** `UX082/packaging-gate.json`当前allowPackaging=false，显示器修正后须在最终源/EXE上完成原生精确像素及保存/复制/保存复制、真实产品输入与文件回读、显示器/绑定回归后才开放。未执行package_preview.py、build_msi.py；out/releases/0.8.2-candidate-001目前仅准备验证脚本，不是包。旧版已交付工件保持历史。

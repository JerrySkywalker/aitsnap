# 用 Cursor 或 AGY 接手：看图与开发入口

> 2026-11-01 Codex 0.7.0 执行更新：实现、完整安装候选、精确检查与未解除门槛见 [DELIVERY-070.md](DELIVERY-070.md)。下文保留原规划/审计标准，不能把未勾选的历史规划误读为本轮没有开发，也不能把组件PASS当成人工验收。

核对日期：2026-11-01。本轮没有在这两个产品中发起会话、上传项目/图片、修改设置或测试模型的实际图像理解。

## 推荐

**这轮以本机Qt界面改造为主，推荐Cursor本地Agent。** 原因是可以在本地IDE并排查看PNG、项目源文件、差异和编译结果，便于Owner继续审计。这是基于当前任务的工作流建议，不是“Cursor模型一定更强”的实测结论，也不建议为此立即购买新订阅。

**AGY（Antigravity CLI）也不等于只能读文字。** 官方当前文档明确支持在输入面板粘贴图片媒体；如果已经熟悉AGY、且有可用额度，可继续用它，不必只因担心不能看草图而换产品。

| 产品 | 官方图片入口 | 本机观察 | 当前证据边界 |
| --- | --- | --- | --- |
| Cursor | 将图片拖入聊天，或Ctrl+V粘贴截图 | 安装文件元数据3.20.21 | 未给本机Agent发送图片，未测试所选模型 |
| AGY CLI | 输入面板Ctrl+V粘贴媒体；文档列出PNG等格式 | `agy --version`为1.2.13 | 当前文档未在本轮证明对这个已安装版本/终端/模型完全适用；应确认图片确实成为附件 |

来源：[Cursor官方图片输入](https://prod.cursor.com/docs/agent/prompting#image-input)、[Antigravity CLI官方媒体附件](https://antigravity.google/docs/cli/prompting/#attaching-media)。这些文档说明产品入口；不等于本机视觉能力或Windows原生UI自动化验收。

## 如何把草图真正交给它

1. 在本地Windows打开 `C:\Dev\src\aitsnap`，核对工作树并读 `OWNER-HANDOFF.md`。不要把同一工作树同时交给多个写入会话。
2. 优先附PNG：`08-display-layout-default.png`、`04-sources-tab.png`及其余相关导航图。**只粘贴一个文件路径、或者只读MD，不能当然认为模型已看到图片像素。** 确认聊天中出现图片附件/预览，再让Agent描述图中可见面板和对应控件。
3. 把SVG和生成脚本作为几何/文案参考，HTML用于人工切换审计；不要把HTML误当成现有Qt实现，或只看SVG文本就自称已看图。
4. 用现有Qt/MSVC本地工具链构建。产品能看PNG/能操作浏览器，不代表能点击原生MSI或验证多屏/Word；这些仍需实际证据，不能绕过工具策略。

## 可直接粘贴的接手任务

```text
在 C:\Dev\src\aitsnap 接手 AITSnap，先只读核对工作树、分支与权限。
阅读 plans/AITSNAP-V1-UX-ITERATION-002/OWNER-HANDOFF.md、
ROADMAP-AND-CHECKLIST.md、TABS-CONCEPT.md、DISPLAY-LAYOUT.md、ACCEPTANCE.md。
同时实际查看我附上的PNG草图，先说明你已看到的布局和可复用控件。

最新设计要求：进入“源区域”默认“显示器布局”，第二子页是“区域浏览”。
现有软件只保存每个配置的布局基线并检测变化，没有完整多环境自动恢复。
不要将设计草图或“文档有字段”说成产品已实现自动布局记忆。

先处理路线中的安装后立即启动失败及准备过程反馈，再推进UI基础和导航页。
优先复用现有区域编辑、SceneEditor/渲染器、批次manifest和ReportQueue；
新增缩略浏览与联动，不重写捕获、Word COM或队列核心。
内部UUID保持稳定，用户界面/未来文件名使用编号与实际名称。
保持原始证据、历史配置及未完成Word任务；预算读取父ledger或交接包保守快照，
不新建一个120小时预算。按有界工作包推进、针对性验证并记录准确SHA与结果。
Windows10/Win7/Word2010或原生人工验收不可得时记HOLD，继续其他独立工作。
```

没有在本轮更换工具、购买服务、传输项目或启动另一个开发Agent。选择工具后，Owner可直接使用上述入口和新版完整交接包。

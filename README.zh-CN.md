# AITSnap 中文说明

[English README](README.md)

AITSnap 是面向截图整理与试验执行记录关联的开源桌面软件项目。
0.6.0 候选版已实现多屏截图、托盘、区域/自由拼图编辑、Word 队列和只读事件关联，提供 Windows Installer 安装包。
使用方法见[操作手册](docs/preview/OPERATOR-MANUAL.zh-CN.md)，范围见[发布说明](docs/preview/RELEASE-NOTES.md)。
本机 Win11 自动化证据不代表 Win10、Win7、Word2010 或人工视觉验收通过。

## 已确定的产品方向

- 多屏、多区域截图。
- OBS 风格的自由拼图画布。
- 截图及关联记录的 Word 自动归档。
- 截图与指令、序列执行的关联。
- Windows 系统托盘入口。
- 可复用组件架构，逐步分离界面和后续能力。

本仓库仅含公开合成夹具，不含真实试验数据、敏感指令载荷或内部型号资料。

## 技术与系统边界

- Windows 7 SP1 恢复为**必需支持平台**，采用 Qt 5.15 Legacy 构建候选；
  x64、x86 均未完成 Win7 原生运行验收。
- Windows 10（1809+）/ Windows 11 x64 保留 Qt 6.12 + MSVC 2022 主构建。
- 保留 C++20、CMake 3.24+、单仓库及共用源码。
  `AITSNAP_QT_MAJOR=5|6` 显式选择 Qt，不自动回退。
- 后续业务核心与 Qt 大版本及原生平台适配层解耦；目前未开发业务功能。
- Linux 仍为未来可能支持，不开发、不测试。

详见[平台矩阵与适配边界](docs/windows-compatibility.md)。必需支持是产品要求，
并不表示 Win7 运行验收已经通过。

## 构建

复用现有 vcpkg，以及官方 Qt 6.12 MSVC x64 SDK，主构建命令保持不变。
在当前 PowerShell 会话中，将 `VCPKG_ROOT` 指向现有 vcpkg 根目录，
将 `QT_ROOT` 指向 Qt 套件安装前缀，然后在仓库根目录执行：

```powershell
cmake --preset windows-msvc-qt-sdk
cmake --build --preset windows-msvc-qt-sdk-release
```

Qt 5 Legacy x64 候选通过 `QT5_ROOT` 指定 Qt 5.15 MSVC x64 套件，使用
`windows-qt5-legacy-x64` / `windows-qt5-legacy-x64-release` 预设。
该候选选择 v142，本机缺少此工具集，目前不能完成该候选构建。
另有 x86 候选，以及明确命名为 `host-preview` 的 v143 本机预验证预设；
本机预验证二进制不能当作已验收的 Win7 发布包。

所有预设均加载同一套现有 vcpkg toolchain；Qt 来自 SDK，并非 vcpkg Qt 包。
继续关闭 manifest 自动安装及 app-local 复制，不新建 vcpkg，不编译 Qt 源码，
不提交本机绝对路径。

配置、运行时 DLL/插件设置及实际 toolchain 加载核验方法见
[Windows 构建说明](docs/build-windows.md)。
[原始初始化验证记录](docs/initial-validation.md) 和
[Qt 6 基线验证记录](docs/qt6-baseline-validation.md) 均保留为历史。
当前平台矩阵取代其中的旧平台政策，不改写历史结果。

## 许可证

项目源码使用 [Apache-2.0](LICENSE)。Qt 作为外部依赖保留其自身许可证，
本仓库不附带 Qt 二进制或其他第三方依赖。

## 架构与开发计划

参见[架构文档索引](docs/ARCHITECTURE-INDEX.md)、[AITSnap V1 Goal Train](plans/AITSNAP-V1-GOAL-TRAIN-001/PROGRAM.md)和[本地执行交接](plans/AITSNAP-V1-GOAL-TRAIN-001/OWNER-HANDOFF.md)。这些文件仅代表批准的实施计划，不代表产品功能已实现或 Windows 7 已通过原生验收。

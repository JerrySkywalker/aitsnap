# AITSnap 中文说明

[English README](README.md)

AITSnap 是面向截图整理与试验执行记录关联的开源桌面软件项目。
当前仍为软件骨架：源码只有一个 Qt Widgets 占位窗口，尚无可用产品功能或安装包。
实际构建与启动检查的范围见[本轮验证记录](docs/qt6-baseline-validation.md)，
不代表全部目标系统或人工视觉验收通过。

## 已确定的产品方向

- 多屏、多区域截图。
- OBS 风格的自由拼图画布。
- 截图及关联记录的 Word 自动归档。
- 截图与指令、序列执行的关联。
- Windows 系统托盘入口。
- 可复用组件架构，逐步分离界面和后续能力。

上述内容均为产品方向，本轮不实现截图、画布、Word、托盘或系统联动功能，
也不开展架构研究。本仓库不含真实测试数据、敏感指令文件或内部型号资料。

## 技术与系统边界

- 正式平台基线：Windows 10（1809 或更高版本）/ Windows 11，x64。
- 技术栈：C++20、Qt 6.12 Widgets、CMake 3.24 或更高版本；链接 `Qt6::Widgets`。
- 首选编译器：MSVC 2022（v143）x64，配套相同 ABI 的 Qt SDK。
- Qt 版本限定为 6.12.x；采用更新次版本前须重新检查平台支持，
  因为 Qt 6.12 是最后支持 Windows 10 的 Qt 版本。
- Windows 7 和 Linux 均仅保留未来可能支持，当前不开发、不测试。
- 正式平台声明与实际验证范围分开记录，不能据此认定所有系统版本已经测试通过。

## 构建

复用现有 vcpkg，以及官方 Qt 6.12 MSVC x64 SDK。
在当前 PowerShell 会话中，将 `VCPKG_ROOT` 指向现有 vcpkg 根目录，
将 `QT_ROOT` 指向 Qt 套件安装前缀，然后在仓库根目录执行：

```powershell
cmake --preset windows-msvc-qt-sdk
cmake --build --preset windows-msvc-qt-sdk-release
```

可移植预设显式加载 `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`，
使用 `x64-windows` triplet。Qt 实际来自 `QT_ROOT` 指定的官方 SDK，
并非来自 vcpkg 的 Qt 包。预设关闭 manifest 自动安装及 vcpkg 的 app-local 复制，
避免配置时触发 Qt 源码构建。不新建另一套 vcpkg，不提交本机绝对路径。

配置、运行时 DLL/插件设置及实际 toolchain 加载核验方法见
[Windows 构建说明](docs/build-windows.md)。
[原始初始化验证记录](docs/initial-validation.md) 完整保留为历史，其中的
Qt 5 和 Windows 7 要求已由本轮基线取代。

## 许可证

项目源码使用 [Apache-2.0](LICENSE)。Qt 作为外部依赖保留其自身许可证，
本仓库不附带 Qt 二进制或其他第三方依赖。

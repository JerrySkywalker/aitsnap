# AITSnap 中文说明

[English README](README.md)

AITSnap 是面向截图整理与试验执行记录关联的开源桌面软件项目。
当前仅完成仓库初始化：源码只有一个 Qt Widgets 占位窗口，尚无可用产品功能、
安装包或经过验证的系统兼容性。

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

- 技术栈：C++20、Qt Widgets、CMake 3.16 或更高版本。
- 初始依赖基线：Qt 5.15，链接 `Qt5::Widgets`。
- 目标系统：Windows 7 SP1、Windows 10/11；目前均未完成系统验收。
- Windows 7 SP1 与 Qt 5.15 的兼容性保留待验证。编译器、Qt 构建、运行库和
  部署依赖需要共同验证，支持 C++20 不等于支持 Windows 7 运行。
- Linux 仅保留未来移植可能，当前没有 Linux 验证结论。

## 构建

准备支持 C++20 的编译器、与编译器 ABI 匹配的 Qt 5.15 开发套件，以及 CMake。
在仓库根目录执行，例如使用 Ninja：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="<qt-kit-prefix>"
cmake --build build
```

将占位符替换为本机 Qt 套件安装前缀，而不是其 `bin` 目录。
本机路径只用于本地配置，不应提交。多配置生成器可使用
`cmake --build build --config Debug` 选择配置。
Windows 上运行程序需要可用的 Qt DLL 和平台插件；本轮不提供打包或部署流程。

实际验证结果见[初始化验证记录](docs/initial-validation.md)。配置检查或编译器探针
通过，不代表 Qt 应用构建通过，也不代表目标系统兼容性通过。

## 许可证

项目源码使用 [Apache-2.0](LICENSE)。Qt 作为外部依赖保留其自身许可证，
本仓库不附带 Qt 二进制或其他第三方依赖。

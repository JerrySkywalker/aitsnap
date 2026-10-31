# Windows 10 / 11 安装候选

目标系统：Windows10 1809（build17763）及以后 x64，或 Windows11 x64。Qt6.12 官方支持这一构建组合。实际应用已在当前 Win11 主机测试；Owner 已确认目前没有 Win10 验收目标，所以 Windows10 原生结果保留 HOLD。

## 安装与启动

1. 双击 `AITSnap-0.6.0-windows-x64.msi`，Windows Installer 将程序安装到当前用户本地应用数据目录的 `Programs\AITSnap`。
2. 从开始菜单 AITSnap 启动。设置、Word 队列与默认截图位于独立的用户数据目录 AITSnap 下，报告路径由操作员明确选择。
3. 打开区域编辑器，确认当前显示器布局，再设置截图区域、保存目录和快捷键。启用 Word 前先选择自己的合成 `.docx` 报告。

安装和运行不需要下载 Qt SDK、Python 或 vcpkg；运行库、平台插件和说明均随包提供。Word 报告功能需要本机合法安装的 Word，未安装 Word 时截图和拼图仍可独立使用。不会自动安装 Office、全局 VC 运行库或设备驱动。

## 修复、升级与卸载

- 修复：对原 MSI 使用 Windows Installer 修复，或执行 `msiexec /fa {ProductCode}`；准确 ProductCode 在安装目录的 `msi-install.json` 中。
- 升级：安装更高版本 MSI。较早版本在同一事务中移除，新版本替换程序文件；配置和外部截图/报告保持独立。较低版本会拒绝覆盖更高版本。
- 卸载：先从托盘退出，再通过“应用和功能/程序和功能”卸载 AITSnap Preview，或使用 `msiexec /x {ProductCode}`。只移除安装器管理的程序文件、入口和注册，不清除安装目录之外的证据。
- 不要对 MSI 目录运行便携包的卸载脚本；脚本会拒绝，以免留下失效的 Windows Installer 注册。

## 验收边界

本轮交付的是 Windows10/11 安装候选；Win10 实机、Word2010、Win7 和 Owner 人工视觉/排版结果未被当前 Win11 自动化测试替代。文件哈希和 MSI 来源记录随执行回执提供，最终以实际目标机回执决定支持范围。

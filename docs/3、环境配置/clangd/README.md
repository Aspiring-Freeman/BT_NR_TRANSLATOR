# Clangd 环境配置模板

本目录存放不同平台的 `.clangd` 配置文件模板。

## 使用方法

1. 根据你的操作系统选择对应模板
2. 复制到项目根目录并重命名为 `.clangd`
3. 根据实际安装路径修改 ARM GCC 头文件路径
4. 重启 VS Code 或执行 "clangd: Restart language server"

## 模板列表

| 文件 | 平台 | 说明 |
|------|------|------|
| `clangd_linux.yaml` | Linux | apt 安装 arm-none-eabi-gcc，路径 `/usr/lib/arm-none-eabi/` |
| `clangd_windows.yaml` | Windows | GNU Arm Embedded Toolchain 10 2021.10 默认安装路径 |

## 注意事项

- `.clangd` 已加入 `.gitignore`，不会被 git 追踪
- Linux 下 clangd 可通过 `--query-driver` 自动查询系统头文件路径，Windows 不行
- 路径获取命令：`arm-none-eabi-gcc -E -v -xc /dev/null` (Linux) 或 `arm-none-eabi-gcc -E -v -xc nul` (Windows)
- **include-cleaner 诊断建议保持开启**，遵循 IWYU（Include What You Use）原则：
	- 聊合/再导出头在其内部使用 `// IWYU pragma: export`
	- 条件编译盲区（`#ifdef` 分支选择不同 .h）在使用处加 `// IWYU pragma: keep`
	- 不要用 `Diagnostics: { MissingIncludes: None, UnusedIncludes: None }` 粗暴闭嘴 — 会掩盖隐藏依赖问题

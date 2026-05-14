# CMake 构建时间戳自动更新问题

## 问题描述

在嵌入式项目中使用 `__DATE__` 和 `__TIME__` 宏来显示固件编译时间：

```c
log_i("Build: %s %s", __DATE__, __TIME__);
```

**问题现象**：增量编译时，如果 `main.c` 没有被修改，编译器不会重新编译该文件，导致固件中的构建时间戳不会更新。

**日志示例**：
```
I/main     [00:00.000] Build: Jan  4 2026 21:04:39   // 时间没有更新！
```

## 原因分析

- `__DATE__` 和 `__TIME__` 是 **预处理器宏**，在文件编译时被替换为当前时间
- CMake/Make 使用 **增量编译**，只重新编译修改过的文件
- 如果 `main.c` 未修改，则不会重新编译，时间戳保持不变

## 解决方案

在 `CMakeLists.txt` 中添加一个自定义目标，在每次构建前 `touch` main.c 文件，强制其重新编译：

```cmake
# ===== FORCE REBUILD MAIN.C FOR TIMESTAMP UPDATE =====
# 强制每次构建时重新编译 main.c，确保 __DATE__ 和 __TIME__ 宏自动更新
# 方法: 在构建前 touch main.c 使其时间戳更新
add_custom_target(touch_main_for_timestamp ALL
    COMMAND ${CMAKE_COMMAND} -E touch_nocreate ${SRC_DIR}/main.c
    COMMENT "Touching main.c to update build timestamp (__DATE__ / __TIME__)"
)
add_dependencies(${PROJECT_NAME} touch_main_for_timestamp)
```

## 关键点说明

| 配置项 | 说明 |
|--------|------|
| `add_custom_target(...ALL)` | `ALL` 确保每次构建都执行此目标 |
| `${CMAKE_COMMAND} -E touch_nocreate` | CMake 跨平台命令，更新文件时间戳 |
| `add_dependencies()` | 确保在编译主目标前先执行 touch |

## 跨平台兼容性

此方案在 **Linux** 和 **Windows** 上都能正常工作：

- `${CMAKE_COMMAND}` → CMake 自动解析为正确的可执行文件
- `-E touch_nocreate` → CMake 内置跨平台命令
- 路径分隔符 → CMake 自动处理 (`/` vs `\`)

## 验证方法

连续两次构建，观察 `main.c` 是否每次都被重新编译：

```bash
# 第一次构建
cmake --build build --target all 2>&1 | grep -E "(main.c|timestamp)"

# 等待后再次构建
sleep 2 && cmake --build build --target all 2>&1 | grep -E "(main.c|timestamp)"
```

预期输出：
```
[  1%] Touching main.c to update build timestamp (__DATE__ / __TIME__)
[ 89%] Building C object CMakeFiles/.../main.c.obj
```

## 注意事项

1. **轻微增加编译时间**：每次构建都会重新编译 `main.c`，但对于小型嵌入式项目影响很小
2. **Git 状态**：`touch` 命令只修改文件时间戳，不修改内容，不会影响 Git 状态
3. **替代方案**：也可以考虑在构建时通过 CMake 生成包含时间戳的头文件

---

**创建日期**: 2026-01-04
**适用项目**: Malaysia WWA LoRaWAN Meter FCT
**CMake 版本**: 3.16+

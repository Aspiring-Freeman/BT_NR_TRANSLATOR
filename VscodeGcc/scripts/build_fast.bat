@echo off
REM ===============================================
REM 快速编译脚本 - Release模式优化编译
REM ===============================================

echo 正在清理和配置快速编译环境...

REM 创建快速编译目录
if not exist "build_fast" mkdir build_fast
cd build_fast

REM 使用Release模式配置CMake（优化编译速度和代码大小）
echo 配置CMake (Release模式)...
cmake -DCMAKE_BUILD_TYPE=Release -G "Ninja" ..
if %ERRORLEVEL% neq 0 (
    echo CMake配置失败！
    pause
    exit /b 1
)

REM 多线程并行编译
echo 开始快速编译 (使用所有可用CPU核心)...
cmake --build . --parallel
if %ERRORLEVEL% neq 0 (
    echo 编译失败！
    pause
    exit /b 1
)

echo.
echo ===============================================
echo 快速编译完成！生成的文件：
echo ===============================================
dir *.elf *.hex *.bin /b
echo.
echo 编译模式: Release (优化后)
echo 位置: build_fast 目录
echo ===============================================
pause

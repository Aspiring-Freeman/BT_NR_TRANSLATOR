@echo off
REM ===============================================
REM 增量编译脚本 - 只编译修改的文件
REM ===============================================

echo 增量编译 - 只编译修改的文件...

REM 检查是否已有构建目录
if not exist "build_fast" (
    echo 首次编译，运行完整构建...
    call build_fast.bat
    exit /b 0
)

cd build_fast

REM 快速增量编译
echo 开始增量编译...
cmake --build . --parallel
if %ERRORLEVEL% neq 0 (
    echo 增量编译失败！
    pause
    exit /b 1
)

echo.
echo ===============================================
echo 增量编译完成！
echo ===============================================
echo 编译时间: 大大缩短（仅编译修改的文件）
echo 位置: build_fast 目录
echo ===============================================
pause

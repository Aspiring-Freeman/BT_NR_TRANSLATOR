@echo off
REM ===============================================
REM 清理重建脚本 - 完全重新编译
REM ===============================================

echo 清理所有构建文件并重新编译...

REM 删除所有构建目录
if exist "build" (
    echo 删除旧的 build 目录...
    rmdir /s /q build
)

if exist "build_fast" (
    echo 删除旧的 build_fast 目录...
    rmdir /s /q build_fast
)

echo 开始全新构建...
call build_fast.bat

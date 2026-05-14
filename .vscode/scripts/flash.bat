@echo off
echo --- Inside flash.bat ---
echo Current directory: %cd%
echo Arguments received: %*

REM 激活虚拟环境 - 请根据你的实际路径修改
call C:\Users\YourUsername\Program\Embeded_work_space\PyOCD\venv\Scripts\activate.bat
echo After venv activation

REM 检查参数
if "%~1"=="" (
    echo Error: No hex file provided
    exit /b 1
)

set HEX_FILE=%~1
echo Hex file: %HEX_FILE%

REM 检查hex文件是否存在
if not exist "%HEX_FILE%" (
    echo Error: Hex file not found: %HEX_FILE%
    exit /b 1
)

REM 显示连接的调试器
echo === Available probes ===
pyocd list

REM 方法1：指定调试器ID，强制使用FM33LG04x目标
set PROBE_ID=07200001005600213500000e4e544d36a5a5a5a597969908
echo === Attempting to flash using ST-Link with FM33LG04x target ===
pyocd flash "%HEX_FILE%" --probe "%PROBE_ID%" --target FM33LG04x --verbose

REM 如果失败，尝试不同的目标变体
if %errorlevel% neq 0 (
    echo === Trying with different FM33LG04x variants ===
    for %%t in (FM33LG048C4P FM33LG048 FM33LG048) do (
        echo Trying target: %%t
        pyocd flash "%HEX_FILE%" --probe "%PROBE_ID%" --target "%%t" --verbose
        if !errorlevel! equ 0 (
            echo Success with target: %%t
            goto :success
        )
    )
)

REM 如果还是失败，尝试使用mbed设备（如果它是你的MM32开发板）
if %errorlevel% neq 0 (
    echo === Trying with mbed device ===
    REM 检查mbed设备是否可用 - Windows下可以用wmic或其他方法
    echo Found potential mbed device, attempting to use it...
    pyocd flash "%HEX_FILE%" --target FM33LG048
)

:success
echo --- Exiting flash.bat ---
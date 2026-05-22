@echo off
REM ===============================================
REM BT_NR_TRANSLATOR - PyOCD GDB Server (Windows)
REM 目标: fm33le01x  |  端口: 3333 (GDB) / 4444 (telnet)
REM ===============================================
setlocal

REM ---- 脚本所在目录 / 项目根目录 ----
set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%..\.."
set "PROJECT_DIR=%CD%"
popd

REM ---- 配置 ----
set "PYOCD_BIN=D:\work\python\pyocd\venv\Scripts\pyocd.exe"
set "TARGET=fm33le01x"
set "FREQUENCY=1000000"
set "PACK_FILE=%PROJECT_DIR%\VscodeGcc\FM33LE0XX_DFP.1.0.2.pack"
set "PORT=3333"
set "TELNET_PORT=4444"

echo === BT_NR_TRANSLATOR Debug Helper ===
echo Project : %PROJECT_DIR%
echo Pack    : %PACK_FILE%
echo Target  : %TARGET%   Freq: %FREQUENCY%
echo Port    : %PORT% (GDB) / %TELNET_PORT% (telnet)
echo.

if not exist "%PYOCD_BIN%" (
    echo [FAIL] pyocd not found: %PYOCD_BIN%
    echo        请修改本脚本顶部 PYOCD_BIN 指向你的 pyocd.exe
    pause
    exit /b 1
)
if not exist "%PACK_FILE%" (
    echo [FAIL] Pack file not found: %PACK_FILE%
    pause
    exit /b 1
)

REM ---- 停掉已存在的 pyocd ----
tasklist /FI "IMAGENAME eq pyocd.exe" 2>NUL | find /I "pyocd.exe" >NUL
if not errorlevel 1 (
    echo [INFO] Stopping existing pyocd ...
    taskkill /F /IM pyocd.exe >NUL 2>&1
    timeout /t 2 /nobreak >NUL
)

REM ---- 启动 GDB Server ----
cd /d "%PROJECT_DIR%"
start /min "PyOCD GDB Server" "%PYOCD_BIN%" gdbserver ^
    --target %TARGET% ^
    --frequency %FREQUENCY% ^
    --pack "%PACK_FILE%" ^
    --port %PORT% ^
    --telnet-port %TELNET_PORT% ^
    --persist

echo.
echo [ OK ] PyOCD GDB Server started on port %PORT%
echo        在 VSCode 中按 F5 (选择 "FM33LE0XX Attach") 开始调试
echo        停止: scripts\stop_debug.bat
pause
endlocal


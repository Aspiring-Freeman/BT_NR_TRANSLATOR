@echo off
echo Starting PyOCD GDB Server...

REM Stop any existing PyOCD processes
taskkill /F /IM pyocd.exe >NUL 2>&1

REM Wait a moment
timeout /t 2 /nobreak >NUL

REM Start PyOCD
cd /d "D:\Work\EmbededSoftware\FM33LG048\WaterMeterTestProgram_AllPlatform"
start /min "" "D:\work\python\pyocd\venv\Scripts\pyocd.exe" gdbserver --target fm33lg04x --frequency 1000000 --pack "D:\Work\EmbededSoftware\FM33LG048\WaterMeterTestProgram_AllPlatform\VscodeGcc\FMSH.FM33LG0XX_DFP.3.0.1.pack" --port 3333 --telnet-port 4444 --persist

echo PyOCD GDB Server started on port 3333
echo You can now debug in VSCode (press F5)
pause

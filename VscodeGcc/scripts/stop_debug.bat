@echo off
echo Stopping PyOCD GDB Server...
taskkill /F /IM pyocd.exe >NUL 2>&1
echo PyOCD stopped.
pause

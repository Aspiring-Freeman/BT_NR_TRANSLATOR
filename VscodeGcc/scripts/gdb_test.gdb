# 用法: arm-none-eabi-gdb -x VscodeGcc/scripts/gdb_test.gdb
# 前提: 先用 scripts/start_debug.bat 或 start_debug.sh 启动了 pyocd gdbserver (port 3333)
file build/BT_PassThroughFT.elf
target remote localhost:3333
monitor reset halt
info registers
detach
quit

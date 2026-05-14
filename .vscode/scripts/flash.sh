#!/bin/bash
echo "--- Inside flash.sh ---"
echo "Current directory: $(pwd)"
echo "Arguments received: $@"

# 激活虚拟环境
source /home/noah/Program/pyOCD/venv/bin/activate
echo "After venv activation"

# 检查参数
if [ -z "$1" ]; then
    echo "Error: No hex file provided"
    exit 1
fi

HEX_FILE="$1"
echo "Hex file: $HEX_FILE"

# 检查hex文件是否存在
if [ ! -f "$HEX_FILE" ]; then
    echo "Error: Hex file not found: $HEX_FILE"
    exit 1
fi

# 显示连接的调试器
echo "=== Available probes ==="
pyocd list

# 方法1：指定调试器ID，强制使用FM33LG04x目标
PROBE_ID="07200001005600213500000e4e544d36a5a5a5a597969908"
echo "=== Attempting to flash using ST-Link with FM33LG04x target ==="
pyocd flash "$HEX_FILE" --probe "$PROBE_ID" --target FM33LG04x --verbose

# 如果失败，尝试不同的目标变体
if [ $? -ne 0 ]; then
    echo "=== Trying with different FM33LG04x variants ==="
    for target in FM33LG048C4P FM33LG048 FM33LG048; do
        echo "Trying target: $target"
        pyocd flash "$HEX_FILE" --probe "$PROBE_ID" --target "$target" --verbose
        if [ $? -eq 0 ]; then
            echo "Success with target: $target"
            break
        fi
    done
fi

# 如果还是失败，尝试使用mbed设备（如果它是你的MM32开发板）
if [ $? -ne 0 ]; then
    echo "=== Trying with mbed device ==="
    # 检查mbed设备是否可用
    if lsusb | grep -q "0d28:0204"; then
        echo "Found mbed device, attempting to use it..."
        # 这里可能需要不同的方法，取决于你的开发板
        pyocd flash "$HEX_FILE" --target FM33LG048
    fi
fi

echo "--- Exiting flash.sh ---"

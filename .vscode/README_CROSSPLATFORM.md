# Cross-Platform VSCode Configuration

This directory contains cross-platform VSCode configurations for the WaterMeterTestProgram project that automatically detect and configure settings for both Linux and Windows platforms.

## Features

- **Automatic Platform Detection**: Configurations automatically adapt based on the detected operating system
- **Dual Platform Support**: Separate configurations for Linux and Windows
- **Integrated Build System**: Platform-specific CMake build tasks
- **Debug Support**: Platform-specific debug configurations for Cortex-Debug
- **PyOCD Integration**: Platform-specific flashing and debugging tools

## Platform-Specific Configurations

### Linux Configuration
- **Compiler Path**: `/usr/bin/arm-none-eabi-gcc`
- **GDB Path**: `/usr/bin/gdb-multiarch`
- **PyOCD Path**: `/home/noah/Program/pyOCD/venv/bin/pyocd`
- **Build Command**: `cmake`

### Windows Configuration
- **Compiler Path**: `C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2021.10/bin/arm-none-eabi-gcc.exe`
- **GDB Path**: `C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2021.10/bin/arm-none-eabi-gdb.exe`
- **PyOCD Path**: `pyocd.exe` (assumes it's in PATH)
- **Build Command**: `cmake.exe`

## Setup Instructions

### Automatic Setup (Recommended)

1. Open the project directory in VSCode
2. Run the platform setup script:
   ```bash
   # Linux/macOS
   python3 .vscode/setup_platform.py

   # Windows
   python .vscode/setup_platform.py
   ```
3. Restart VSCode to apply the configuration changes

### Manual Setup

If you prefer manual configuration or need to customize paths:

1. **Check Platform Detection**: VSCode will automatically select the appropriate configuration from `c_cpp_properties.json` based on your OS
2. **Verify Toolchain Paths**: Ensure the compiler and debugger paths in the configuration files match your system
3. **Update PyOCD Path**: Modify the PyOCD paths in `tasks.json` if your installation differs

## Available Tasks

### Linux Tasks
- `Build: CMake Default (Linux)` - Build the project using CMake
- `PyOCD: Flash (Linux)` - Flash the firmware using PyOCD
- `PyOCD: Erase (Linux)` - Erase the chip using PyOCD
- `PyOCD: Start GDB Server (Linux)` - Start PyOCD GDB server
- `Build: Clean (Linux)` - Clean build artifacts
- `Build: Configure (Linux)` - Configure CMake build
- `Memory Usage Analysis (Linux)` - Analyze memory usage

### Windows Tasks
- `Build: CMake Default (Windows)` - Build the project using CMake
- `PyOCD: Flash (Windows)` - Flash the firmware using PyOCD
- `PyOCD: Erase (Windows)` - Erase the chip using PyOCD
- `PyOCD: Start GDB Server (Windows)` - Start PyOCD GDB server
- `Build: Clean (Windows)` - Clean build artifacts
- `Build: Configure (Windows)` - Configure CMake build
- `Memory Usage Analysis (Windows)` - Analyze memory usage

## Debug Configurations

### Linux Debug Configurations
- `FM33LG0XX Debug (Linux - External Server)` - Debug using external GDB server
- `FM33LG0XX Attach (Linux)` - Attach to running target

### Windows Debug Configurations
- `FM33LG0XX Debug (Windows - External Server)` - Debug using external GDB server
- `FM33LG0XX Attach (Windows)` - Attach to running target

## Customization

### Modifying Toolchain Paths

If your toolchain is installed in a different location, update the paths in:

1. **c_cpp_properties.json**: Update `compilerPath` in the appropriate platform configuration
2. **launch.json**: Update `armToolchainPath` and `gdbPath` in debug configurations
3. **tasks.json**: Update tool paths in the build and analysis tasks

### Adding New Platforms

To add support for additional platforms:

1. Add a new configuration block in `c_cpp_properties.json`
2. Add corresponding debug configurations in `launch.json`
3. Add platform-specific tasks in `tasks.json`
4. Update the `setup_platform.py` script to handle the new platform

## Troubleshooting

### Common Issues

1. **Toolchain Not Found**: Verify that the ARM GCC toolchain is installed and paths are correct
2. **PyOCD Not Found**: Ensure PyOCD is installed and accessible in your PATH or update the absolute paths
3. **Configuration Not Active**: VSCode should automatically select the platform-appropriate configuration, but you can manually select it in the C/C++ extension settings

### Getting Help

1. Check the VSCode Output panel for detailed error messages
2. Verify that all required extensions are installed:
   - C/C++ Extension Pack
   - Cortex-Debug
   - CMake Tools
3. Run the setup script with verbose output to diagnose configuration issues

## File Structure

```
.vscode/
├── c_cpp_properties.json    # Cross-platform C/C++ configurations
├── launch.json             # Cross-platform debug configurations
├── tasks.json              # Cross-platform build tasks
├── settings.json           # General VSCode settings
├── setup_platform.py       # Automatic platform setup script
├── scripts/                # Flash and debug scripts
│   ├── flash.sh            # Linux flash script
│   └── flash.bat           # Windows flash script
└── README_CROSSPLATFORM.md # This documentation
```

## Notes

- The configuration assumes standard installation paths for toolchains. Adjust paths as needed for your system.
- PyOCD virtual environment path is hardcoded for Linux. Update if your setup differs.
- The setup is optimized for FM33LG048 microcontroller development.
- All configurations use the same SVD file and target settings for consistency.

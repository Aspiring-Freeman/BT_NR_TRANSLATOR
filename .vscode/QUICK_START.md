# Quick Start Guide - Cross-Platform VSCode Setup

## 🚀 Quick Setup (Recommended)

1. **Open the project in VSCode**:
   ```bash
   code /path/to/WaterMeterTestProgram_AllPlatform
   ```

2. **Run the platform setup script**:
   ```bash
   # Linux/macOS
   python3 .vscode/setup_platform.py
   
   # Windows
   python .vscode/setup_platform.py
   ```

3. **Restart VSCode** to apply changes

4. **You're ready to go!** VSCode will automatically use the correct platform configuration.

## 🔧 What Gets Configured Automatically

### Linux Users
- ✅ ARM GCC toolchain: `/usr/bin/arm-none-eabi-gcc`
- ✅ GDB debugger: `/usr/bin/gdb-multiarch`
- ✅ PyOCD flashing tool: Your custom PyOCD path
- ✅ Build tasks: Linux-specific CMake commands

### Windows Users
- ✅ ARM GCC toolchain: Standard Windows ARM toolchain path
- ✅ GDB debugger: Windows ARM GDB path
- ✅ PyOCD flashing tool: System PATH PyOCD
- ✅ Build tasks: Windows-specific CMake commands

## 🎯 Available Commands

After setup, you'll have these platform-appropriate commands:

### Build Commands
- **Ctrl+Shift+P** → `Tasks: Run Task` → `Build: CMake Default (Linux/Windows)`
- **Ctrl+Shift+P** → `Tasks: Run Task` → `Build: Configure (Linux/Windows)`
- **Ctrl+Shift+P** → `Tasks: Run Task` → `Build: Clean (Linux/Windows)`

### Flash Commands
- **Ctrl+Shift+P** → `Tasks: Run Task` → `PyOCD: Flash (Linux/Windows)`
- **Ctrl+Shift+P** → `Tasks: Run Task` → `PyOCD: Erase (Linux/Windows)`

### Debug Commands
- **F5** → Select `FM33LG0XX Debug (Linux/Windows - External Server)`
- **Ctrl+Shift+P** → `Debug: Start Debugging` → Select your platform configuration

## 🔍 Verification

### Test Build
1. Open terminal in VSCode (**Ctrl+`**)
2. Run: 
   ```bash
   # Linux
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   
   # Windows
   cmake.exe -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake.exe --build build
   ```

### Test Configuration
1. Open any `.c` file in the project
2. Check that syntax highlighting and IntelliSense work
3. Hover over functions - you should see documentation
4. **Ctrl+Space** should show autocomplete suggestions

## 🛠️ Troubleshooting

### Common Issues

**"Toolchain not found"**
- Linux: Install ARM GCC: `sudo apt install gcc-arm-none-eabi`
- Windows: Install ARM GCC toolchain and update paths in configs

**"PyOCD not found"**
- Linux: Check your PyOCD installation path and update `tasks.json`
- Windows: Ensure PyOCD is in your system PATH

**"Configuration not active"**
- Run the setup script again: `python3 .vscode/setup_platform.py`
- Restart VSCode
- Check C/C++ extension configuration

### Platform-Specific Notes

**Linux Users:**
- Ensure you have `gdb-multiarch` installed
- PyOCD path is hardcoded to your virtual environment - update if needed
- ARM toolchain should be in `/usr/bin/`

**Windows Users:**
- Default paths assume standard ARM toolchain installation
- Update paths in configuration files if your installation differs
- Ensure PyOCD is accessible from command line

## 📁 Configuration Files

The setup creates these files:
- `c_cpp_properties.json` - IntelliSense and compiler settings
- `launch.json` - Debug configurations
- `tasks.json` - Build and flash tasks
- `settings.json` - VSCode workspace settings

All files contain both Linux and Windows configurations - VSCode automatically selects the right one based on your platform.

## 🔄 Switching Platforms

If you work on both platforms:
1. Copy the entire project to the other platform
2. Run the setup script on the new platform
3. VSCode will automatically use the correct configuration

No manual switching needed! 🎉

## 📚 More Information

For detailed configuration and customization options, see:
- `README_CROSSPLATFORM.md` - Complete documentation
- Individual configuration files for platform-specific settings

---

**Happy coding!** 🚀

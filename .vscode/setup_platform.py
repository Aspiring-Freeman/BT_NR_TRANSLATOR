#!/usr/bin/env python3
"""
Cross-platform VSCode configuration setup script for WaterMeterTestProgram
Automatically detects the platform and configures VSCode settings accordingly.
"""

import os
import platform
import json
import shutil

def detect_platform():
    """Detect the current platform."""
    system = platform.system().lower()
    if system == "windows":
        return "windows"
    elif system == "linux":
        return "linux"
    elif system == "darwin":
        return "macos"
    else:
        return "unknown"

def setup_vscode_config(project_root):
    """Setup VSCode configuration based on the detected platform."""
    vscode_dir = os.path.join(project_root, ".vscode")
    
    if not os.path.exists(vscode_dir):
        print(f"Error: .vscode directory not found at {vscode_dir}")
        return False
    
    current_platform = detect_platform()
    print(f"Detected platform: {current_platform}")
    
    # Read current configurations
    c_cpp_properties_file = os.path.join(vscode_dir, "c_cpp_properties.json")
    launch_file = os.path.join(vscode_dir, "launch.json")
    tasks_file = os.path.join(vscode_dir, "tasks.json")
    
    try:
        # Update c_cpp_properties.json to set the active configuration
        if os.path.exists(c_cpp_properties_file):
            with open(c_cpp_properties_file, 'r', encoding='utf-8') as f:
                c_cpp_config = json.load(f)
            
            # Find the appropriate configuration name
            if current_platform == "windows":
                config_name = "Win32"
            elif current_platform == "linux":
                config_name = "Linux"
            else:
                config_name = "Linux"  # Default to Linux for unknown platforms
            
            # Check if the configuration exists
            config_exists = any(config['name'] == config_name for config in c_cpp_config['configurations'])
            
            if config_exists:
                # Set the active configuration (this is done by VSCode automatically based on platform)
                print(f"Platform-specific configuration '{config_name}' is available")
            else:
                print(f"Warning: Configuration '{config_name}' not found in c_cpp_properties.json")
        
        # Create a platform-specific settings file
        platform_settings_file = os.path.join(vscode_dir, f"settings_{current_platform}.json")
        settings_file = os.path.join(vscode_dir, "settings.json")
        
        if current_platform == "linux":
            # Linux-specific settings
            platform_settings = {
                "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
                "cortex-debug.armToolchainPath": "/usr/bin",
                "cortex-debug.gdbPath": "/usr/bin/gdb-multiarch"
            }
        elif current_platform == "windows":
            # Windows-specific settings
            platform_settings = {
                "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
                "cortex-debug.armToolchainPath": "C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2021.10/bin",
                "cortex-debug.gdbPath": "C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2021.10/bin/arm-none-eabi-gdb.exe"
            }
        else:
            platform_settings = {}
        
        # Read existing settings and merge
        existing_settings = {}
        if os.path.exists(settings_file):
            with open(settings_file, 'r', encoding='utf-8') as f:
                existing_settings = json.load(f)
        
        # Merge platform-specific settings
        existing_settings.update(platform_settings)
        
        # Write updated settings
        with open(settings_file, 'w', encoding='utf-8') as f:
            json.dump(existing_settings, f, indent=4)
        
        print(f"VSCode configuration updated for {current_platform} platform")
        
        # Print available tasks and launch configurations for the current platform
        print(f"\nAvailable configurations for {current_platform}:")
        
        if current_platform == "linux":
            print("- Debug: FM33LG0XX Debug (Linux - External Server)")
            print("- Attach: FM33LG0XX Attach (Linux)")
            print("- Build: Build: CMake Default (Linux)")
            print("- Flash: PyOCD: Flash (Linux)")
        elif current_platform == "windows":
            print("- Debug: FM33LG0XX Debug (Windows - External Server)")
            print("- Attach: FM33LG0XX Attach (Windows)")
            print("- Build: Build: CMake Default (Windows)")
            print("- Flash: PyOCD: Flash (Windows)")
        
        return True
        
    except Exception as e:
        print(f"Error setting up VSCode configuration: {e}")
        return False

def main():
    """Main function."""
    # Get the project root directory (parent of .vscode)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    
    print("=== Cross-Platform VSCode Setup ===")
    print(f"Project root: {project_root}")
    
    success = setup_vscode_config(project_root)
    
    if success:
        print("\n✅ Setup completed successfully!")
        print("\nNext steps:")
        print("1. Restart VSCode to apply the new configuration")
        print("2. Select the appropriate build and debug configurations for your platform")
        print("3. Configure your toolchain paths if they differ from the defaults")
    else:
        print("\n❌ Setup failed. Please check the error messages above.")
        return 1
    
    return 0

if __name__ == "__main__":
    exit(main())

cmake_minimum_required(VERSION 3.16)

# ===== TOOLCHAIN CONFIGURATION =====
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

# FM33LE0xx 使用 Cortex-M0 核心
set(CPU_FLAGS "-mcpu=cortex-m0 -mthumb -mfloat-abi=soft")

set(CMAKE_C_FLAGS "${CPU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS "${CPU_FLAGS} -ffunction-sections -fdata-sections -fno-rtti -fno-exceptions")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CPU_FLAGS} -specs=nano.specs -specs=nosys.specs -Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# ===== PROJECT DEFINITION =====
# 版本号唯一真源：App/main.c 中的 SOFTWARE_VERSION_{MAJOR,MINOR,PATCH}
set(_VERSION_SOURCE "${CMAKE_SOURCE_DIR}/App/app_config.h")
file(READ "${_VERSION_SOURCE}" _ver_content)
string(REGEX MATCH "#define[ \t]+SOFTWARE_VERSION_MAJOR[ \t]+([0-9]+)" _ "${_ver_content}")
set(_VER_MAJOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define[ \t]+SOFTWARE_VERSION_MINOR[ \t]+([0-9]+)" _ "${_ver_content}")
set(_VER_MINOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define[ \t]+SOFTWARE_VERSION_PATCH[ \t]+([0-9]+)" _ "${_ver_content}")
set(_VER_PATCH "${CMAKE_MATCH_1}")
if(NOT _VER_MAJOR OR _VER_MINOR STREQUAL "" OR _VER_PATCH STREQUAL "")
    message(FATAL_ERROR "无法从 ${_VERSION_SOURCE} 解析 SOFTWARE_VERSION_{MAJOR,MINOR,PATCH}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_VERSION_SOURCE}")

project(BT_PassThroughFT
    VERSION ${_VER_MAJOR}.${_VER_MINOR}.${_VER_PATCH}
    DESCRIPTION "BT PassThrough FunctionTest for FM33LE015"
    LANGUAGES C CXX ASM
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_EXECUTABLE_SUFFIX .elf)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== PROJECT DIRECTORIES =====
set(DRIVERS_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/Drivers)
set(CMSIS_DIR       ${DRIVERS_DIR}/CMSIS/Device)
set(FM33_DIR        ${DRIVERS_DIR}/FM33LE0xx_FL_Driver)
set(CONFIG_DIR      ${CMAKE_CURRENT_SOURCE_DIR}/MF-config)
set(APP_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/App)
set(BSP_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/Bsp)
set(COMPONENTS_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/Components)

# ===== LINKER SCRIPT =====
set(LINKER_SCRIPT ${CMSIS_DIR}/FM/FM33xx/Source/Templates/gcc/linker/fm33le01x_flash.ld)
if(NOT EXISTS ${LINKER_SCRIPT})
    message(FATAL_ERROR "Linker script not found: ${LINKER_SCRIPT}")
endif()

# ===== GIT VERSION INFO =====
set(BUILD_INFO_DIR    ${CMAKE_BINARY_DIR}/generated)
set(BUILD_INFO_HEADER ${BUILD_INFO_DIR}/build_info.h)
file(MAKE_DIRECTORY ${BUILD_INFO_DIR})

add_custom_target(generate_build_info ALL
    COMMAND ${CMAKE_COMMAND}
        -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DDST_OUTPUT=${BUILD_INFO_HEADER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_build_info.cmake
    BYPRODUCTS ${BUILD_INFO_HEADER}
    COMMENT "Regenerating build_info.h -> ${BUILD_INFO_HEADER}"
)

# ===== EXECUTABLE =====
add_executable(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMSIS_DIR}
    ${CMSIS_DIR}/FM/FM33xx/Include
    ${FM33_DIR}/Inc
    ${APP_DIR}
    ${BSP_DIR}
    ${BSP_DIR}/Peripheral/uart
    ${CONFIG_DIR}/Inc
    ${BUILD_INFO_DIR}
    # Components
    ${COMPONENTS_DIR}/TimeManager
    ${COMPONENTS_DIR}/EasyLogger
    ${COMPONENTS_DIR}/EasyLogger/easylogger/inc
    ${COMPONENTS_DIR}/Utility
    ${COMPONENTS_DIR}/
    # FlashDB includes
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/FlashDB
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/FlashDB/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/FlashDB/port/fal/inc

    # Protocol framework
    ${COMPONENTS_DIR}/Protocol
    ${COMPONENTS_DIR}/Protocol/PC
    ${COMPONENTS_DIR}/Protocol/App/BT_Passthrough

    # RTT库
    ${COMPONENTS_DIR}/RTT/RTT
    ${COMPONENTS_DIR}/RTT/Config
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    DEBUG
    FM33LE0XX
    $<$<CONFIG:Debug>:DEBUG=1>
    $<$<CONFIG:Release>:NDEBUG=1>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-but-set-variable
    -Wno-unused-variable
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2 -DNDEBUG>
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>
)

target_link_options(${PROJECT_NAME} PRIVATE
    -T${LINKER_SCRIPT}
    -Wl,--print-memory-usage
    -Wl,-Map=${PROJECT_NAME}.map
    -Wl,--cref
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    # 抑制 newer ld 对裸机工程默认产生的 RWX LOAD segment 提醒
    -Wl,--no-warn-rwx-segments
)

# ===== SOURCE FILES =====
set(CMSIS_SOURCES
    ${CMSIS_DIR}/FM/FM33xx/Source/Templates/gcc/startup_fm33le0xx.s
    ${CMSIS_DIR}/FM/FM33xx/Source/system_fm33le0xx.c
)

file(GLOB FM33_DRIVER_SOURCES
    ${FM33_DIR}/Src/fm33le0xx_fl*.c
)

# 显式列出编译文件（避免 glob 拉入旧文件）
set(APP_SOURCES
    ${APP_DIR}/main.c
    ${APP_DIR}/ble.c
)

set(BSP_SOURCES
    ${BSP_DIR}/GPIO.c
    ${BSP_DIR}/GPTIM.c
    ${BSP_DIR}/LED_CTRL.c
    ${BSP_DIR}/elog_port.c
    ${BSP_DIR}/syscalls_stub.c
    ${BSP_DIR}/LPTIM.c
    ${BSP_DIR}/Peripheral/uart/pc_uart.c
    ${BSP_DIR}/Peripheral/uart/ble_uart.c
)

set(CONFIG_SOURCES
    ${CONFIG_DIR}/Src/mf_config.c
)

set(PROTOCOL_SOURCES
    ${COMPONENTS_DIR}/Protocol/protocol_manager.c
    ${COMPONENTS_DIR}/Protocol/upgrade_magic.c
    ${COMPONENTS_DIR}/Protocol/upgrade_storage.c
    ${COMPONENTS_DIR}/Protocol/system_reset.c
    ${COMPONENTS_DIR}/Protocol/App/BT_Passthrough/bt_ds809.c
    ${COMPONENTS_DIR}/Protocol/App/BT_Passthrough/bt_app_proto.c
    # PC 端协议实现 (与 components.h 中启用的宏对应)
    ${COMPONENTS_DIR}/Protocol/PC/pc_protocol_common.c
    ${COMPONENTS_DIR}/Protocol/PC/pc_protocol_upgrade.c
    ${COMPONENTS_DIR}/Protocol/PC/pc_protocol_config.c
)

set(COMPONENT_SOURCES
    ${COMPONENTS_DIR}/TimeManager/time_manager.c
    ${COMPONENTS_DIR}/TimeManager/retry_manager.c
    ${COMPONENTS_DIR}/components.c
)

set(UTILITY_SOURCES)
file(GLOB UTILITY_SOURCES ${COMPONENTS_DIR}/Utility/*.c)

# EasyLogger 核心源文件
set(EASYLOGGER_SOURCES
    ${COMPONENTS_DIR}/EasyLogger/easylogger/src/elog.c
    ${COMPONENTS_DIR}/EasyLogger/easylogger/src/elog_utils.c
)

#FlashDB源文件
set(FLASHDB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/FlashDB/src/fdb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/FlashDB/src/fdb_kvdb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/FlashDB/src/fdb_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/FlashDB/port/fal/src/fal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/FlashDB/port/fal/src/fal_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/FlashDB/port/fal/src/fal_partition.c
    # FM33LE015 Flash port
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/FlashDB/fal_flash_fm33le015_port.c
    # Flash diagnostics and test stats
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/FlashDB/flash_diag.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Components/FlashDB/test_stats.c
)

set(RTT_SOURCES
    ${COMPONENTS_DIR}/RTT/RTT/SEGGER_RTT.c
    ${COMPONENTS_DIR}/RTT/RTT/SEGGER_RTT_printf.c
)

target_sources(${PROJECT_NAME} PRIVATE
    ${CMSIS_SOURCES}
    ${FM33_DRIVER_SOURCES}
    ${APP_SOURCES}
    ${BSP_SOURCES}
    ${CONFIG_SOURCES}
    ${PROTOCOL_SOURCES}
    ${COMPONENT_SOURCES}
    ${FLASHDB_SOURCES}
    ${UTILITY_SOURCES}
    ${EASYLOGGER_SOURCES}
    ${RTT_SOURCES}
)

add_dependencies(${PROJECT_NAME} generate_build_info)

# ===== DRIVER STATIC LIBRARY =====
add_library(fm33_drivers STATIC ${FM33_DRIVER_SOURCES})
target_include_directories(fm33_drivers PUBLIC
    ${CMSIS_DIR}
    ${CMSIS_DIR}/FM/FM33xx/Include
    ${FM33_DIR}/Inc
)

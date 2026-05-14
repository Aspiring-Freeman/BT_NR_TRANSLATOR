/**
 * @file components.c
 * @brief 组件统一初始化实现
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "components.h"

#ifdef COMPONENT_EASYLOGGER
#include <elog.h>
#endif

/*============================================================================
 * 版本信息
 *===========================================================================*/

#define COMPONENTS_VERSION "1.0.0"

/*============================================================================
 * 内部函数声明
 *===========================================================================*/

static bool init_protocol_system(void);

/*============================================================================
 * API 实现
 *===========================================================================*/

bool Components_Init(const ComponentsConfig *config) {
  bool result = true;

#ifdef COMPONENT_EASYLOGGER
  // EasyLogger 通常在这之前已经初始化，这里只是标记
  // elog_init() 和 elog_start() 应该在串口初始化后立即调用
#endif

#ifdef COMPONENT_PROTOCOL_MANAGER
  result = init_protocol_system();
  if (!result) {
    return false;
  }

  // 协议注册完成后，设置所有回调函数（确保顺序正确）
  if (config != NULL) {
    Components_SetSendFunctions(config->pc_send, config->bluetooth_send);
    Components_SetVersionFunc(config->get_version);
    Components_SetBuildTimeFunc(config->get_build_time);
    Components_SetFTControlFunc(config->ft_control);
  }

#endif

  return result;

#ifdef COMPONENT_FLASHDB
  // FlashDB / FAL 初始化
  if (fal_init() >= 0) {
    elog_i("components", "FAL分区表初始化成功");
    // 打印Flash分区信息
    FlashDiag_Init();
    FlashDiag_PrintInfo();
  } else {
    elog_e("components", "FAL初始化失败");
  }

  // 测试统计初始化
  if (TestStats_Init()) {
    elog_i("components", "测试统计初始化成功");
  } else {
    elog_w("components", "测试统计初始化失败 (首次使用正常)");
  }
#endif

#ifdef COMPONENT_UPGRADE_STORAGE
  // 升级参数存储初始化
  if (UpgradeStorage_Init()) {
    elog_i("components", "升级存储初始化成功");
  } else {
    elog_w("components", "升级存储初始化失败 (首次使用正常)");
  }
#endif

  return result;
}

void Components_SetSendFunctions(void (*pc_send)(uint8_t *, uint16_t),
                                 void (*device_send)(uint8_t *, uint16_t)) {
#ifdef COMPONENT_PROTOCOL_MANAGER
  if (pc_send != NULL) {
    Protocol_SetPCSendFunc(pc_send);
  }
  if (device_send != NULL) {
    Protocol_SetDeviceSendFunc(device_send);
  }
#else
  (void)pc_send;
  (void)device_send;
#endif
}

void Components_SetStationIdFunc(uint8_t (*get_station_id)(void)) {
#ifdef COMPONENT_PROTOCOL_MANAGER
  // 设置PC协议层的工位号获取回调
  PC_Protocol_SetStationIdFunc(get_station_id);
#else
  (void)get_station_id;
#endif
}

void Components_SetFTControlFunc(void (*ft_control)(const uint8_t *,
                                                    uint16_t)) {
#ifdef COMPONENT_PROTOCOL_MANAGER
  // 设置PC协议层的工装控制回调
  PC_Protocol_SetFTControlFunc(ft_control);
#else
  (void)ft_control;
#endif
}

void Components_SetFailInfoFunc(uint8_t (*get_fail_info)(uint8_t *, char *,
                                                         uint8_t *, char *)) {
#ifdef COMPONENT_PROTOCOL_MANAGER
  // 设置PC协议层的失败信息获取回调
  PC_Protocol_SetFailInfoFunc(get_fail_info);
#else
  (void)get_fail_info;
#endif
}

void Components_SetVersionFunc(uint16_t (*get_version)(void)) {
#ifdef COMPONENT_PROTOCOL_MANAGER
  // 设置PC协议层的版本号获取回调
  PC_Protocol_SetGetVersionFunc(get_version);
#else
  (void)get_version;
#endif
}

void Components_SetBuildTimeFunc(void (*get_build_time)(char *)) {
#ifdef COMPONENT_PROTOCOL_MANAGER
  // 设置PC协议层的编译时间获取回调
  PC_Protocol_SetGetBuildTimeFunc(get_build_time);
#else
  (void)get_build_time;
#endif
}

const char *Components_GetVersion(void) { return COMPONENTS_VERSION; }

void Components_PrintInfo(void) {
#ifdef COMPONENT_EASYLOGGER
  elog_i("components", "========== 组件配置信息 ==========");
  elog_i("components", "组件框架版本: %s", COMPONENTS_VERSION);
  elog_i("components", "已启用组件:");

#ifdef COMPONENT_PROTOCOL_MANAGER
  elog_i("components", "  ✓ 协议管理器");
#endif

#ifdef COMPONENT_PC_PROTOCOL_DIAPHRAGM_GAS_METER
  elog_i("components", "  ✓ 膜式燃气表协议");
#endif

#ifdef COMPONENT_PROTOCOL_UPGRADE
  elog_i("components", "  ✓ APP升级协议");
#endif

#ifdef COMPONENT_PROTOCOL_WATER_METER
  elog_i("components", "  ✓ 水表协议");
#endif

#ifdef COMPONENT_PROTOCOL_LEGACY
  elog_i("components", "  ✓ Legacy协议");
#endif

#ifdef COMPONENT_VALVE_CTRL
  elog_i("components", "  ✓ 阀门控制");
#endif

#ifdef COMPONENT_FLASHDB
  elog_i("components", "  ✓ FlashDB");
  elog_i("components", "  ✓ Flash诊断");
  elog_i("components", "  ✓ 测试统计存储");
#endif

#ifdef COMPONENT_UPGRADE_STORAGE
  elog_i("components", "  ✓ 升级参数存储");
#endif

  elog_i("components", "===================================");
#endif
}

/*============================================================================
 * 内部函数实现
 *===========================================================================*/

static bool init_protocol_system(void) {
#ifdef COMPONENT_PROTOCOL_MANAGER
  // 1. 初始化协议管理器
  Protocol_Init();

  // 2. 注册PC端水表协议
#ifdef COMPONENT_PC_PROTOCOL_WATER_METER
  Protocol_RegisterPC(&water_meter_pc_protocol);
#endif

// 注册膜式燃气表协议，暂时先用这个，后续需要注册另外协议，当前先测试，后续迁移
#ifdef COMPONENT_PC_PROTOCOL_DIAPHRAGM_GAS_METER
  Protocol_RegisterPC(&diaphragm_gas_meter_pc_protocol);
#endif
// 注册蓝牙通讯协议
#ifdef COMPONENT_PC_PROTOCOL_BT_PASSTHROUGH
  Protocol_RegisterPC(&bt_passthrough_pc_protocol);
#endif

// 注册升级协议
#ifdef COMPONENT_PROTOCOL_UPGRADE
  Protocol_RegisterPC(&upgrade_pc_protocol);
#endif

#ifdef COMPONENT_PC_PROTOCOL_CONFIG
  Protocol_RegisterPC(&config_pc_protocol);
#endif

#ifdef COMPONENT_PROTOCOL_LEGACY
  Protocol_RegisterPC(&legacy_pc_protocol);
#endif

  // 3. 注册设备端协议
#ifdef COMPONENT_DEVICE_PROTOCOL_WATER_METER
  Protocol_RegisterDevice(&water_meter_protocol);
#endif

  // 4. 设置默认活跃协议 (可选)
#ifdef COMPONENT_DEFAULT_PC_PROTOCOL
  // Protocol_SetActivePC(COMPONENT_DEFAULT_PC_PROTOCOL);
  // 注意: 如果只注册了一个PC协议，会自动激活，无需手动设置
#endif

#ifdef COMPONENT_DEFAULT_DEVICE_PROTOCOL
  // Protocol_SetActiveDevice(COMPONENT_DEFAULT_DEVICE_PROTOCOL);
#endif

  return true;
#else
  return true;
#endif
}

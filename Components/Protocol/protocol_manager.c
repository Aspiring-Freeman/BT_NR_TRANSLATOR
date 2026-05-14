/**
 * @file protocol_manager.c
 * @brief 协议管理器实现
 * @version 1.0.0
 * @date 2024-12
 */

#define LOG_TAG "proto_mgr"

#include "protocol_manager.h"
#include <elog.h>
#include <stdio.h>

/*============ 内部数据结构 ============*/

// 协议注册表项
typedef struct {
  const ProtocolInterface *interface;
  bool registered;
} ProtocolEntry;

// 协议管理器上下文
typedef struct {
  // 上位机协议注册表
  ProtocolEntry pc_protocols[MAX_REGISTERED_PROTOCOLS];
  uint8_t pc_count;
  int8_t active_pc_index;

  // 设备协议注册表
  ProtocolEntry device_protocols[MAX_REGISTERED_PROTOCOLS];
  uint8_t device_count;
  int8_t active_device_index;

  // 发送函数
  ProtocolSendFunc pc_send_func;
  ProtocolSendFunc device_send_func; // 原始底层发送函数(不带前导)
  ProtocolSendFunc device_send_func_raw; // 保存原始函数用于前导发送

  // 初始化标志
  bool initialized;
} ProtocolManagerContext;

// 全局管理器实例
static ProtocolManagerContext g_manager = {0};

// 外部延时函数声明 (由底层提供)
extern void FL_DelayMs(uint32_t ms);

#ifdef ENABLE_WATCHDOG
#include "fm33lg0xx_fl_iwdt.h"
extern IWDT_Type *IWDT;
#endif

/*============ 内部函数 ============*/

/**
 * @brief 发送前导码
 * @param preamble 前导配置
 */
static void send_preamble(const ProtocolPreambleConfig *preamble) {
  if (preamble == NULL || !preamble->enabled ||
      g_manager.device_send_func_raw == NULL) {
    return;
  }

  // 发送主前导码
  for (uint8_t i = 0; i < preamble->repeat_count; i++) {
    g_manager.device_send_func_raw((uint8_t *)preamble->data, preamble->length);

    // 等待发送完成并延时
    if (preamble->delay_ms > 0) {
      FL_DelayMs(preamble->delay_ms);
    }

#ifdef ENABLE_WATCHDOG
    FL_IWDT_ReloadCounter(IWDT);
#endif
  }

  // 发送同步前导码 (如果有)
  if (preamble->sync_data != NULL && preamble->sync_length > 0) {
    g_manager.device_send_func_raw((uint8_t *)preamble->sync_data,
                                   preamble->sync_length);
  }
}

/**
 * @brief 带前导的设备发送包装函数
 * @param data 数据
 * @param len 长度
 */
static void device_send_with_preamble(uint8_t *data, uint16_t len) {
  // 获取当前活跃设备协议的前导配置
  if (g_manager.active_device_index >= 0) {
    const ProtocolInterface *protocol =
        g_manager.device_protocols[g_manager.active_device_index].interface;
    if (protocol != NULL && protocol->preamble != NULL) {
      send_preamble(protocol->preamble);
    }
  }

  // 发送实际数据
  if (g_manager.device_send_func_raw != NULL) {
    g_manager.device_send_func_raw(data, len);
  }
}

/**
 * @brief 在协议表中查找指定名称的协议,没查到说明协议不存在，就需要注册
 * @param entries 协议表
 * @param count 协议数量
 * @param name 协议名称
 * @return 找到返回索引，否则返回-1
 */
static int8_t find_protocol_by_name(const ProtocolEntry *entries, uint8_t count,
                                    const char *name) {
  if (name == NULL) {
    return -1;
  }

  for (uint8_t i = 0; i < count; i++) {
    if (entries[i].registered && entries[i].interface != NULL) {
      if (strcmp(entries[i].interface->name, name) == 0) {
        return (int8_t)i;
      }
    }
  }
  return -1;
}

/*============ 公共接口实现 ============*/

void ProtocolManager_Init(void) {
  // 清零所有数据
  memset(&g_manager, 0, sizeof(g_manager));

  g_manager.active_pc_index = -1;
  g_manager.active_device_index = -1;
  g_manager.initialized = true;

  log_i("协议管理器初始化完成");
}

bool ProtocolManager_RegisterPC(const ProtocolInterface *protocol) {
  if (!g_manager.initialized) {
    log_e("协议管理器未初始化");
    return false;
  }

  if (protocol == NULL || protocol->name == NULL) {
    log_e("无效的协议指针");
    return false;
  }

  if (g_manager.pc_count >= MAX_REGISTERED_PROTOCOLS) {
    log_e("PC协议注册表已满");
    return false;
  }

  // 检查是否已注册
  if (find_protocol_by_name(g_manager.pc_protocols, g_manager.pc_count,
                            protocol->name) >= 0) {
    log_w("PC协议 [%s] 已注册", protocol->name);
    return false;
  }

  // 注册协议
  g_manager.pc_protocols[g_manager.pc_count].interface = protocol;
  g_manager.pc_protocols[g_manager.pc_count].registered = true;
  g_manager.pc_count++;

  // 初始化协议
  if (protocol->init != NULL) {
    protocol->init();
  }

  // 设置发送函数
  if (protocol->set_send_func != NULL && g_manager.pc_send_func != NULL) {
    protocol->set_send_func(g_manager.pc_send_func);
  }

  log_i("注册PC协议: [%s] (共%d个)", protocol->name, g_manager.pc_count);

  // 如果是第一个协议，自动设为活跃
  if (g_manager.pc_count == 1) {
    g_manager.active_pc_index = 0;
    log_i("自动激活PC协议: [%s]", protocol->name);
  }

  return true;
}

bool ProtocolManager_RegisterDevice(const ProtocolInterface *protocol) {
  if (!g_manager.initialized) {
    log_e("协议管理器未初始化");
    return false;
  }

  if (protocol == NULL || protocol->name == NULL) {
    log_e("无效的协议指针");
    return false;
  }

  if (g_manager.device_count >= MAX_REGISTERED_PROTOCOLS) {
    log_e("设备协议注册表已满");
    return false;
  }

  // 检查是否已注册
  if (find_protocol_by_name(g_manager.device_protocols, g_manager.device_count,
                            protocol->name) >= 0) {
    log_w("设备协议 [%s] 已注册", protocol->name);
    return false;
  }

  // 注册协议
  g_manager.device_protocols[g_manager.device_count].interface = protocol;
  g_manager.device_protocols[g_manager.device_count].registered = true;
  g_manager.device_count++;

  // 初始化协议
  if (protocol->init != NULL) {
    protocol->init();
  }

  // 设置发送函数 (使用带前导处理的包装函数)
  if (protocol->set_send_func != NULL &&
      g_manager.device_send_func_raw != NULL) {
    protocol->set_send_func(device_send_with_preamble);
  }

  log_i("注册设备协议: [%s] (共%d个)%s", protocol->name, g_manager.device_count,
        (protocol->preamble && protocol->preamble->enabled) ? " [带前导]" : "");

  // 如果是第一个协议，自动设为活跃
  if (g_manager.device_count == 1) {
    g_manager.active_device_index = 0;
    log_i("自动激活设备协议: [%s]", protocol->name);
  }

  return true;
}

bool ProtocolManager_SetActivePC(const char *name) {
  int8_t index =
      find_protocol_by_name(g_manager.pc_protocols, g_manager.pc_count, name);
  if (index < 0) {
    log_e("未找到PC协议: [%s]", name);
    return false;
  }

  g_manager.active_pc_index = index;
  log_i("切换PC协议: [%s]", name);
  return true;
}

bool ProtocolManager_SetActiveDevice(const char *name) {
  int8_t index = find_protocol_by_name(g_manager.device_protocols,
                                       g_manager.device_count, name);
  if (index < 0) {
    log_e("未找到设备协议: [%s]", name);
    return false;
  }

  g_manager.active_device_index = index;
  log_i("切换设备协议: [%s]", name);
  return true;
}

const char *ProtocolManager_GetActivePCName(void) {
  if (g_manager.active_pc_index < 0) {
    return NULL;
  }
  return g_manager.pc_protocols[g_manager.active_pc_index].interface->name;
}

const char *ProtocolManager_GetActiveDeviceName(void) {
  if (g_manager.active_device_index < 0) {
    return NULL;
  }
  return g_manager.device_protocols[g_manager.active_device_index]
      .interface->name;
}

uint8_t ProtocolManager_GetPCCount(void) { return g_manager.pc_count; }

uint8_t ProtocolManager_GetDeviceCount(void) { return g_manager.device_count; }

const char *ProtocolManager_GetPCNameByIndex(uint8_t index) {
  if (index >= g_manager.pc_count) {
    return NULL;
  }
  return g_manager.pc_protocols[index].interface->name;
}

const char *ProtocolManager_GetDeviceNameByIndex(uint8_t index) {
  if (index >= g_manager.device_count) {
    return NULL;
  }
  return g_manager.device_protocols[index].interface->name;
}

/**
 * @brief PC协议解析 - 轮询认领模式
 *
 * 遍历所有注册的PC协议，让每个协议尝试解析数据。
 * 谁认领成功（返回OK）谁处理，其他协议跳过。
 *
 * 优点：
 * - 无需手动切换"活跃协议"
 * - MES和Upgrade可以完美共存
 * - 上位机可以混发不同类型的指令
 *
 * 要求：各协议的命令字不能重复（MES用AA/AC/AE，Upgrade用BA/BB）
 */
ProtocolResult ProtocolManager_PC_Parse(uint8_t *data, uint16_t len) {
  if (g_manager.pc_count == 0) {
    log_e("没有注册任何PC协议");
    return PROTOCOL_RESULT_ERROR;
  }

  // 轮询所有注册的PC协议，让每个协议尝试认领
  for (uint8_t i = 0; i < g_manager.pc_count; i++) {
    const ProtocolInterface *protocol = g_manager.pc_protocols[i].interface;

    if (protocol == NULL || protocol->parse == NULL) {
      continue;
    }

    // 让协议尝试解析
    ProtocolResult result = protocol->parse(data, len);

    if (result == PROTOCOL_RESULT_OK) {
      // 协议认领成功，处理完毕
      log_d("PC协议 [%s] 认领并处理成功", protocol->name);
      return PROTOCOL_RESULT_OK;
    }

    // 如果返回 INCOMPLETE，说明数据不完整，等待更多数据
    if (result == PROTOCOL_RESULT_INCOMPLETE) {
      return PROTOCOL_RESULT_INCOMPLETE;
    }

    // 其他结果（ERROR/UNKNOWN_CMD等）表示该协议不认识这个包，继续问下一个
  }

  // 所有协议都不认识这个包
  log_w("所有PC协议都无法识别此数据包");
  return PROTOCOL_RESULT_UNKNOWN_CMD;
}

/**
 * @brief 设备协议解析 - 轮询认领模式
 *
 * 与PC协议相同的轮询认领逻辑
 */
ProtocolResult ProtocolManager_Device_Parse(uint8_t *data, uint16_t len) {
  if (g_manager.device_count == 0) {
    log_e("没有注册任何设备协议");
    return PROTOCOL_RESULT_ERROR;
  }

  // 轮询所有注册的设备协议
  for (uint8_t i = 0; i < g_manager.device_count; i++) {
    const ProtocolInterface *protocol = g_manager.device_protocols[i].interface;

    if (protocol == NULL || protocol->parse == NULL) {
      continue;
    }

    ProtocolResult result = protocol->parse(data, len);

    if (result == PROTOCOL_RESULT_OK) {
      log_d("设备协议 [%s] 认领并处理成功", protocol->name);
      return PROTOCOL_RESULT_OK;
    }

    if (result == PROTOCOL_RESULT_INCOMPLETE) {
      return PROTOCOL_RESULT_INCOMPLETE;
    }
  }

  log_w("所有设备协议都无法识别此数据包");
  return PROTOCOL_RESULT_UNKNOWN_CMD;
}

bool ProtocolManager_PC_SendCmd(uint16_t cmd, void *param) {
  if (g_manager.active_pc_index < 0) {
    log_e("没有活跃的PC协议");
    return false;
  }

  const ProtocolInterface *protocol =
      g_manager.pc_protocols[g_manager.active_pc_index].interface;

  if (protocol->send_cmd == NULL) {
    log_e("PC协议 [%s] 未实现send_cmd", protocol->name);
    return false;
  }

  return protocol->send_cmd(cmd, param);
}

bool ProtocolManager_Device_SendCmd(uint16_t cmd, void *param) {
  if (g_manager.active_device_index < 0) {
    log_e("没有活跃的设备协议");
    return false;
  }

  const ProtocolInterface *protocol =
      g_manager.device_protocols[g_manager.active_device_index].interface;

  if (protocol->send_cmd == NULL) {
    log_e("设备协议 [%s] 未实现send_cmd", protocol->name);
    return false;
  }

  return protocol->send_cmd(cmd, param);
}

void ProtocolManager_PC_OnResponse(uint16_t code, const uint8_t *data,
                                   uint16_t len) {
  if (g_manager.active_pc_index < 0) {
    return;
  }

  const ProtocolInterface *protocol =
      g_manager.pc_protocols[g_manager.active_pc_index].interface;

  if (protocol->on_response != NULL) {
    protocol->on_response(code, data, len);
  }
}

void ProtocolManager_Device_OnResponse(uint16_t code, const uint8_t *data,
                                       uint16_t len) {
  if (g_manager.active_device_index < 0) {
    return;
  }

  const ProtocolInterface *protocol =
      g_manager.device_protocols[g_manager.active_device_index].interface;

  if (protocol->on_response != NULL) {
    protocol->on_response(code, data, len);
  }
}

void ProtocolManager_SetPCSendFunc(ProtocolSendFunc send_func) {
  g_manager.pc_send_func = send_func;

  // 更新所有已注册协议的发送函数
  for (uint8_t i = 0; i < g_manager.pc_count; i++) {
    if (g_manager.pc_protocols[i].registered &&
        g_manager.pc_protocols[i].interface->set_send_func != NULL) {
      g_manager.pc_protocols[i].interface->set_send_func(send_func);
    }
  }
}

void ProtocolManager_SetDeviceSendFunc(ProtocolSendFunc send_func) {
  // 保存原始发送函数，用于前导发送
  g_manager.device_send_func_raw = send_func;
  // 使用带前导处理的包装函数
  g_manager.device_send_func = device_send_with_preamble;

  // 更新所有已注册设备协议的发送函数 (使用包装函数)
  for (uint8_t i = 0; i < g_manager.device_count; i++) {
    if (g_manager.device_protocols[i].registered &&
        g_manager.device_protocols[i].interface->set_send_func != NULL) {
      g_manager.device_protocols[i].interface->set_send_func(
          device_send_with_preamble);
    }
  }
}

void ProtocolManager_PrintInfo(void) {
  log_i("========== 协议管理器信息 ==========");
  log_i("PC协议 (共%d个):", g_manager.pc_count);
  for (uint8_t i = 0; i < g_manager.pc_count; i++) {
    const char *active = (i == g_manager.active_pc_index) ? " [活跃]" : "";
    log_i("  [%d] %s%s", i, g_manager.pc_protocols[i].interface->name, active);
  }

  log_i("设备协议 (共%d个):", g_manager.device_count);
  for (uint8_t i = 0; i < g_manager.device_count; i++) {
    const char *active = (i == g_manager.active_device_index) ? " [活跃]" : "";
    log_i("  [%d] %s%s", i, g_manager.device_protocols[i].interface->name,
          active);
  }
  log_i("====================================");
}

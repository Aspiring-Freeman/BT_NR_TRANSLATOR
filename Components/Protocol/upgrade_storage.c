/**
 * @file upgrade_storage.c
 * @brief 升级参数Flash存储实现
 * @version 1.0.0
 * @date 2026-01-05
 *
 * 使用FlashDB的FAL层直接操作Flash分区
 * 存储升级参数供Bootloader读取
 */

#define LOG_TAG "upgrade_storage"

#include "upgrade_storage.h"
#include <elog.h>
#include <fal.h>
#include <string.h>

/*============================================================================
 * 内部定义
 *===========================================================================*/

/* 分区名称 */
#define UPGRADE_PARTITION_NAME "upgrade_params"

/* 静态变量 */
static const struct fal_partition *s_upgrade_part = NULL;
static bool s_initialized = false;

/*============================================================================
 * CRC32 计算 (简化版)
 *===========================================================================*/

static uint32_t calc_crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc >>= 1;
      }
    }
  }
  return ~crc;
}

/*============================================================================
 * API 实现
 *===========================================================================*/

bool UpgradeStorage_Init(void) {
  if (s_initialized) {
    return true;
  }

  /* 初始化FAL */
  if (fal_init() < 0) {
    log_e("FAL初始化失败");
    return false;
  }

  /* 查找升级参数分区 */
  s_upgrade_part = fal_partition_find(UPGRADE_PARTITION_NAME);
  if (s_upgrade_part == NULL) {
    log_e("找不到分区: %s", UPGRADE_PARTITION_NAME);
    return false;
  }

  log_i("升级存储初始化成功, 分区大小=%dB", s_upgrade_part->len);
  s_initialized = true;
  return true;
}

bool UpgradeStorage_SaveParams(uint8_t station_id, uint8_t mode,
                               uint8_t baud_cfg, uint8_t protocol,
                               uint8_t timeout, uint8_t log_en,
                               uint16_t fw_size_kb, uint8_t vendor,
                               uint16_t chip) {
  if (!s_initialized) {
    log_e("模块未初始化");
    return false;
  }

  UpgradeStorageData_t data = {0};

  /* 填充数据 */
  data.magic = UPGRADE_STORAGE_MAGIC;
  data.version = UPGRADE_STORAGE_VERSION;
  data.station_id = station_id;
  data.upgrade_mode = mode;
  data.baud_config = baud_cfg;
  data.protocol = protocol;
  data.timeout_sec = timeout;
  data.log_enable = log_en;
  data.fw_size_kb = fw_size_kb;
  data.vendor_code = vendor;
  data.chip_code = chip;
  data.upgrade_flag = UPGRADE_FLAG_UPGRADE; /* 设置升级标志 */

  /* 计算CRC (不包括checksum字段本身) */
  data.checksum =
      calc_crc32((const uint8_t *)&data, sizeof(data) - sizeof(data.checksum));

  /* 擦除分区 (至少擦除一个扇区) */
  if (fal_partition_erase(s_upgrade_part, 0, s_upgrade_part->len) < 0) {
    log_e("擦除分区失败");
    return false;
  }

  /* 写入数据 */
  if (fal_partition_write(s_upgrade_part, 0, (const uint8_t *)&data,
                          sizeof(data)) < 0) {
    log_e("写入数据失败");
    return false;
  }

  /* 验证写入 */
  UpgradeStorageData_t verify = {0};
  if (fal_partition_read(s_upgrade_part, 0, (uint8_t *)&verify,
                         sizeof(verify)) < 0) {
    log_e("验证读取失败");
    return false;
  }

  if (memcmp(&data, &verify, sizeof(data)) != 0) {
    log_e("数据验证失败");
    return false;
  }

  log_i("升级参数已保存到Flash");
  log_i("  工位=%d, 模式=%s, 波特率=%s, 超时=%ds", station_id,
        mode ? "自动" : "手动", baud_cfg ? "115200" : "9600", timeout);
  log_i("  固件大小=%dKB, 芯片=0x%04X", fw_size_kb, chip);

  return true;
}

bool UpgradeStorage_ReadParams(UpgradeStorageData_t *data) {
  if (!s_initialized || data == NULL) {
    return false;
  }

  /* 读取数据 */
  if (fal_partition_read(s_upgrade_part, 0, (uint8_t *)data, sizeof(*data)) <
      0) {
    log_e("读取数据失败");
    return false;
  }

  /* 验证魔数 */
  if (data->magic != UPGRADE_STORAGE_MAGIC) {
    log_d("魔数无效: 0x%08lX", (unsigned long)data->magic);
    return false;
  }

  /* 验证版本 */
  if (data->version != UPGRADE_STORAGE_VERSION) {
    log_w("版本不匹配: %d != %d", data->version, UPGRADE_STORAGE_VERSION);
    /* 暂时允许，以后可能需要版本迁移 */
  }

  /* 验证CRC */
  uint32_t calc_crc =
      calc_crc32((const uint8_t *)data, sizeof(*data) - sizeof(data->checksum));
  if (calc_crc != data->checksum) {
    log_e("CRC校验失败: 0x%08lX != 0x%08lX", (unsigned long)calc_crc,
          (unsigned long)data->checksum);
    return false;
  }

  return true;
}

bool UpgradeStorage_SetUpgradeFlag(uint8_t flag) {
  UpgradeStorageData_t data = {0};

  if (!UpgradeStorage_ReadParams(&data)) {
    /* 如果没有有效数据，创建一个默认的 */
    data.magic = UPGRADE_STORAGE_MAGIC;
    data.version = UPGRADE_STORAGE_VERSION;
  }

  data.upgrade_flag = flag;

  /* 重新计算CRC */
  data.checksum =
      calc_crc32((const uint8_t *)&data, sizeof(data) - sizeof(data.checksum));

  /* 擦除并写入 */
  if (fal_partition_erase(s_upgrade_part, 0, s_upgrade_part->len) < 0) {
    return false;
  }

  if (fal_partition_write(s_upgrade_part, 0, (const uint8_t *)&data,
                          sizeof(data)) < 0) {
    return false;
  }

  log_i("升级标志已设置为: %s", flag ? "升级模式" : "正常启动");
  return true;
}

uint8_t UpgradeStorage_GetUpgradeFlag(void) {
  UpgradeStorageData_t data = {0};

  if (!UpgradeStorage_ReadParams(&data)) {
    return UPGRADE_FLAG_NORMAL;
  }

  return data.upgrade_flag;
}

bool UpgradeStorage_Clear(void) {
  if (!s_initialized) {
    return false;
  }

  if (fal_partition_erase(s_upgrade_part, 0, s_upgrade_part->len) < 0) {
    log_e("擦除分区失败");
    return false;
  }

  log_i("升级参数已清除");
  return true;
}

bool UpgradeStorage_HasPendingUpgrade(void) {
  return UpgradeStorage_GetUpgradeFlag() == UPGRADE_FLAG_UPGRADE;
}

/*============================================================================
 * 弱符号函数实现 - 供 pc_protocol_upgrade.c 调用
 *===========================================================================*/

/**
 * @brief 保存升级参数到Flash (供升级协议调用)
 *
 * 这个函数实现了 pc_protocol_upgrade.c 中声明的弱符号
 *
 * @param params 指向 UpgradeCommandFrame 结构体的指针
 */
void save_upgrade_params_to_flash(void *params) {
  /* 检查初始化 */
  if (!s_initialized) {
    if (!UpgradeStorage_Init()) {
      log_e("升级存储初始化失败，无法保存参数");
      return;
    }
  }

  /* params 是 UpgradeCommandFrame 类型，需要提取字段 */
  /* 这里假设结构体布局如下:
   * [0] head
   * [1] cmd
   * [2] length
   * [3-6] magic (prefix, vendor, chip_lo, chip_hi)
   * [7] station_id
   * [8] mode
   * [9] baud_cfg
   * [10] protocol
   * [11] timeout_sec
   * [12] log_enable
   * [13-14] fw_size_kb (小端)
   */
  const uint8_t *frame = (const uint8_t *)params;

  uint8_t vendor = frame[4];
  uint16_t chip = frame[5] | (frame[6] << 8);
  uint8_t station_id = frame[7];
  uint8_t mode = frame[8];
  uint8_t baud_cfg = frame[9];
  uint8_t protocol = frame[10];
  uint8_t timeout = frame[11];
  uint8_t log_en = frame[12];
  uint16_t fw_size_kb = frame[13] | (frame[14] << 8);

  UpgradeStorage_SaveParams(station_id, mode, baud_cfg, protocol, timeout,
                            log_en, fw_size_kb, vendor, chip);
}

/**
 * @file flash_diag.c
 * @brief Flash诊断和分区信息查询实现
 * @version 1.0.0
 * @date 2026-01-05
 */

#define LOG_TAG "flash_diag"

#include "flash_diag.h"
#include <elog.h>
#include <fal.h>
#include <string.h>

/* 使用 elog 的日志宏，避免与 FAL 的 log_i 冲突 */
#undef log_i
#undef log_d
#undef log_w
#undef log_e
#define log_i(...) elog_i(LOG_TAG, __VA_ARGS__)
#define log_d(...) elog_d(LOG_TAG, __VA_ARGS__)
#define log_w(...) elog_w(LOG_TAG, __VA_ARGS__)
#define log_e(...) elog_e(LOG_TAG, __VA_ARGS__)

/*============================================================================
 * 内部变量
 *===========================================================================*/

static bool s_initialized = false;

/*============================================================================
 * API 实现
 *===========================================================================*/

bool FlashDiag_Init(void) {
  if (s_initialized) {
    return true;
  }

  /* FAL应该已经在components.c中初始化了 */
  s_initialized = true;
  return true;
}

bool FlashDiag_GetInfo(FlashDiagInfo_t *info) {
  if (info == NULL) {
    return false;
  }

  memset(info, 0, sizeof(FlashDiagInfo_t));

  info->total_size = FLASH_TOTAL_SIZE;
  info->sector_size = FLASH_SECTOR_SIZE;

  /* 填充分区信息 */
  info->partition_count = 5;

  /* Bootloader */
  info->partitions[0].name = "bootloader";
  info->partitions[0].addr = FLASH_BOOTLOADER_ADDR;
  info->partitions[0].size = FLASH_BOOTLOADER_SIZE;
  info->partitions[0].valid = 1; /* 假设有效 */

  /* APP */
  info->partitions[1].name = "app";
  info->partitions[1].addr = FLASH_APP_ADDR;
  info->partitions[1].size = FLASH_APP_SIZE;
  info->partitions[1].valid = 1;

  /* Test Stats */
  info->partitions[2].name = "test_stats";
  info->partitions[2].addr = FLASH_TEST_STATS_ADDR;
  info->partitions[2].size = FLASH_TEST_STATS_SIZE;
  info->partitions[2].valid = FlashDiag_ValidatePartition("test_stats");

  /* Upgrade Params */
  info->partitions[3].name = "upgrade_params";
  info->partitions[3].addr = FLASH_UPGRADE_ADDR;
  info->partitions[3].size = FLASH_UPGRADE_SIZE;
  info->partitions[3].valid = FlashDiag_ValidatePartition("upgrade_params");

  /* KVDB */
  info->partitions[4].name = "kvdb";
  info->partitions[4].addr = FLASH_KVDB_ADDR;
  info->partitions[4].size = FLASH_KVDB_SIZE;
  info->partitions[4].valid = FlashDiag_ValidatePartition("kvdb");

  return true;
}

void FlashDiag_PrintInfo(void) {
  log_i("+------------------------------------------------------------+");
  log_i("|            FM33LG04x Flash Partition Info                  |");
  log_i("+------------------------------------------------------------+");
  log_i("| Total: 256KB   Sector: 2KB   Page: 512B                    |");
  log_i("+----------------+-----------------------+--------+----------+");
  log_i("| Partition      | Address Range         | Size   | Status   |");
  log_i("+----------------+-----------------------+--------+----------+");
  log_i("| bootloader     | 0x00000 - 0x03FFF     | 16KB   | --       |");
  log_i("| app            | 0x04000 - 0x3BFFF     | 224KB  | --       |");
  log_i("| test_stats     | 0x3C000 - 0x3DFFF     | 8KB    | %-8s |",
        FlashDiag_ValidatePartition("test_stats") ? "Valid" : "Empty");
  log_i("| upgrade_params | 0x3E000 - 0x3EFFF     | 4KB    | %-8s |",
        FlashDiag_ValidatePartition("upgrade_params") ? "Valid" : "Empty");
  log_i("| kvdb           | 0x3F000 - 0x3FFFF     | 4KB    | %-8s |",
        FlashDiag_ValidatePartition("kvdb") ? "Valid" : "Empty");
  log_i("+----------------+-----------------------+--------+----------+");
}

int FlashDiag_ReadPartition(const char *partition_name, uint32_t offset,
                            uint8_t *buf, uint32_t size) {
  const struct fal_partition *part = fal_partition_find(partition_name);
  if (part == NULL) {
    log_e("找不到分区: %s", partition_name);
    return -1;
  }

  if (offset + size > part->len) {
    log_e("读取超出分区范围");
    return -1;
  }

  return fal_partition_read(part, offset, buf, size);
}

bool FlashDiag_ValidatePartition(const char *partition_name) {
  const struct fal_partition *part = fal_partition_find(partition_name);
  if (part == NULL) {
    return false;
  }

  /* 读取前16字节检查是否全为0xFF (空白) */
  uint8_t header[16];
  if (fal_partition_read(part, 0, header, sizeof(header)) < 0) {
    return false;
  }

  /* 检查是否全为0xFF */
  bool all_ff = true;
  for (int i = 0; i < (int)sizeof(header); i++) {
    if (header[i] != 0xFF) {
      all_ff = false;
      break;
    }
  }

  /* 如果全为0xFF，说明是空白或已擦除 */
  if (all_ff) {
    return false;
  }

  /* 检查是否有有效的魔数 */
  uint32_t magic =
      header[0] | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);

  /* 检查常见的有效魔数 */
  if (magic == 0x55AA55AA || /* upgrade_params 魔数 */
      magic == 0x464C4442 || /* "FLDB" - FlashDB 魔数 */
      magic == 0x54455354) { /* "TEST" - test_stats 魔数 */
    return true;
  }

  /* 如果不是全0xFF且有数据，也认为有效 */
  return true;
}

bool FlashDiag_ErasePartition(const char *partition_name) {
  const struct fal_partition *part = fal_partition_find(partition_name);
  if (part == NULL) {
    log_e("找不到分区: %s", partition_name);
    return false;
  }

  log_w("正在擦除分区: %s (大小=%lu字节)", partition_name,
        (unsigned long)part->len);

  if (fal_partition_erase(part, 0, part->len) < 0) {
    log_e("擦除失败");
    return false;
  }

  log_i("分区 %s 已擦除", partition_name);
  return true;
}

int FlashDiag_ReadRaw(uint32_t addr, uint8_t *buf, uint32_t size) {
  if (addr + size > FLASH_TOTAL_SIZE) {
    return -1;
  }

  /* 直接内存读取 */
  memcpy(buf, (const void *)addr, size);
  return size;
}

void FlashDiag_DumpHex(uint32_t addr, uint32_t size) {
  if (size > 256) {
    size = 256;
  }

  uint8_t buf[256];
  int read_size = FlashDiag_ReadRaw(addr, buf, size);
  if (read_size < 0) {
    log_e("读取失败");
    return;
  }

  log_i("Flash内容 @ 0x%05lX (大小=%lu):", (unsigned long)addr,
        (unsigned long)size);

  /* 每行16字节 */
  for (uint32_t i = 0; i < size; i += 16) {
    char line[80];
    int pos = 0;

    /* 地址 */
    pos += snprintf(line + pos, sizeof(line) - pos,
                    "%05lX: ", (unsigned long)(addr + i));

    /* HEX 数据 */
    for (uint32_t j = 0; j < 16 && (i + j) < size; j++) {
      pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", buf[i + j]);
    }

    /* 补齐空格 */
    for (uint32_t j = size - i; j < 16 && (i + j) >= size; j++) {
      pos += snprintf(line + pos, sizeof(line) - pos, "   ");
    }

    /* ASCII */
    pos += snprintf(line + pos, sizeof(line) - pos, " |");
    for (uint32_t j = 0; j < 16 && (i + j) < size; j++) {
      char c = buf[i + j];
      pos += snprintf(line + pos, sizeof(line) - pos, "%c",
                      (c >= 32 && c < 127) ? c : '.');
    }
    pos += snprintf(line + pos, sizeof(line) - pos, "|");

    log_i("%s", line);
  }
}

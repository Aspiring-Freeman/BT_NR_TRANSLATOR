/**
 * @file upgrade_magic.c
 * @brief 通用固件升级协议魔数实现
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "upgrade_magic.h"
#include <string.h>

/*============================================================================
 * 芯片信息表
 *===========================================================================*/

/**
 * @brief 支持的芯片信息表
 *
 * 注意: 表格按 vendor_code 和 chip_code 排序，便于二分查找（可选优化）
 */
static const ChipInfo_t s_chip_table[] = {
    /*========== 复旦微 (FMSH) ==========*/
    {.vendor_code = CHIP_VENDOR_FMSH,
     .chip_code = FMSH_FM33LG04X,
     .flash_size = 256 * 1024, /* 256KB */
     .flash_start = 0x00000000,
     .page_size = 512,
     .sector_size = 2048,          /* 2KB */
     .bootloader_size = 16 * 1024, /* 16KB */
     .app_start = 0x00004000,      /* 16KB offset */
     .name = "FM33LG04x"},
    {.vendor_code = CHIP_VENDOR_FMSH,
     .chip_code = FMSH_FM33LG08X,
     .flash_size = 512 * 1024, /* 512KB */
     .flash_start = 0x00000000,
     .page_size = 512,
     .sector_size = 2048,
     .bootloader_size = 16 * 1024,
     .app_start = 0x00004000,
     .name = "FM33LG08x"},
    {.vendor_code = CHIP_VENDOR_FMSH,
     .chip_code = FMSH_FM33LC04X,
     .flash_size = 256 * 1024,
     .flash_start = 0x00000000,
     .page_size = 512,
     .sector_size = 2048,
     .bootloader_size = 16 * 1024,
     .app_start = 0x00004000,
     .name = "FM33LC04x"},

    /*========== 兆易创新 (GD) ==========*/
    {.vendor_code = CHIP_VENDOR_GD,
     .chip_code = GD_GD32F103C8,
     .flash_size = 64 * 1024, /* 64KB */
     .flash_start = 0x08000000,
     .page_size = 1024,
     .sector_size = 0, /* 无扇区概念，按页操作 */
     .bootloader_size = 16 * 1024,
     .app_start = 0x08004000,
     .name = "GD32F103C8"},
    {.vendor_code = CHIP_VENDOR_GD,
     .chip_code = GD_GD32F303RC,
     .flash_size = 256 * 1024,
     .flash_start = 0x08000000,
     .page_size = 2048,
     .sector_size = 0,
     .bootloader_size = 16 * 1024,
     .app_start = 0x08004000,
     .name = "GD32F303RC"},

    /*========== 意法半导体 (ST) ==========*/
    {.vendor_code = CHIP_VENDOR_ST,
     .chip_code = ST_STM32F103C8,
     .flash_size = 64 * 1024,
     .flash_start = 0x08000000,
     .page_size = 1024,
     .sector_size = 0,
     .bootloader_size = 16 * 1024,
     .app_start = 0x08004000,
     .name = "STM32F103C8"},
    {.vendor_code = CHIP_VENDOR_ST,
     .chip_code = ST_STM32F103RC,
     .flash_size = 256 * 1024,
     .flash_start = 0x08000000,
     .page_size = 2048,
     .sector_size = 0,
     .bootloader_size = 16 * 1024,
     .app_start = 0x08004000,
     .name = "STM32F103RC"},
    {.vendor_code = CHIP_VENDOR_ST,
     .chip_code = ST_STM32F407VE,
     .flash_size = 512 * 1024,
     .flash_start = 0x08000000,
     .page_size = 0,           /* F4使用扇区，无固定页大小 */
     .sector_size = 16 * 1024, /* 前4个扇区16KB */
     .bootloader_size = 16 * 1024,
     .app_start = 0x08004000,
     .name = "STM32F407VE"},

    /*========== 沁恒 (WCH) ==========*/
    {.vendor_code = CHIP_VENDOR_WCH,
     .chip_code = WCH_CH32V103C8,
     .flash_size = 64 * 1024,
     .flash_start = 0x08000000,
     .page_size = 256,
     .sector_size = 0,
     .bootloader_size = 8 * 1024,
     .app_start = 0x08002000,
     .name = "CH32V103C8"},
    {.vendor_code = CHIP_VENDOR_WCH,
     .chip_code = WCH_CH32V203C8,
     .flash_size = 64 * 1024,
     .flash_start = 0x08000000,
     .page_size = 256,
     .sector_size = 0,
     .bootloader_size = 8 * 1024,
     .app_start = 0x08002000,
     .name = "CH32V203C8"},
};

/** @brief 芯片信息表大小 */
#define CHIP_TABLE_SIZE (sizeof(s_chip_table) / sizeof(s_chip_table[0]))

/*============================================================================
 * 厂商名称表
 *===========================================================================*/

static const char *s_vendor_names[] = {
    [CHIP_VENDOR_UNKNOWN] = "Unknown",     [CHIP_VENDOR_FMSH] = "FMSH",
    [CHIP_VENDOR_GD] = "GigaDevice",       [CHIP_VENDOR_ST] = "ST",
    [CHIP_VENDOR_NUVOTON] = "Nuvoton",     [CHIP_VENDOR_WCH] = "WCH",
    [CHIP_VENDOR_APM] = "Geehy",           [CHIP_VENDOR_MM] = "MindMotion",
    [CHIP_VENDOR_AT] = "Artery",           [CHIP_VENDOR_HOLYCHIP] = "HolyChip",
    [CHIP_VENDOR_HDSC] = "HDSC",           [CHIP_VENDOR_NATION] = "Nations",
    [CHIP_VENDOR_ESPRESSIF] = "Espressif", [CHIP_VENDOR_BK] = "Beken",
    [CHIP_VENDOR_REALTEK] = "Realtek",
};

#define VENDOR_NAMES_SIZE (sizeof(s_vendor_names) / sizeof(s_vendor_names[0]))

/*============================================================================
 * 当前芯片信息 (静态存储)
 *===========================================================================*/

static const ChipInfo_t s_current_chip = {.vendor_code = CURRENT_CHIP_VENDOR,
                                          .chip_code = CURRENT_CHIP_CODE,
                                          .flash_size =
                                              256 * 1024, /* FM33LG04x: 256KB */
                                          .flash_start = 0x00000000,
                                          .page_size = 512,
                                          .sector_size = 2048,
                                          .bootloader_size = 16 * 1024,
                                          .app_start = 0x00004000,
                                          .name = "FM33LG04x"};

/*============================================================================
 * API实现
 *===========================================================================*/

/**
 * @brief 根据魔数查找芯片信息
 */
const ChipInfo_t *Upgrade_FindChipInfo(const UpgradeMagic_t *magic) {
  if (magic == NULL) {
    return NULL;
  }

  /* 验证前缀 */
  if (magic->prefix != UPGRADE_MAGIC_PREFIX) {
    return NULL;
  }

  return Upgrade_FindChipByCode(magic->vendor, magic->chip);
}

/**
 * @brief 根据厂商和型号代码查找芯片信息
 */
const ChipInfo_t *Upgrade_FindChipByCode(uint8_t vendor, uint16_t chip) {
  for (size_t i = 0; i < CHIP_TABLE_SIZE; i++) {
    if (s_chip_table[i].vendor_code == vendor &&
        s_chip_table[i].chip_code == chip) {
      return &s_chip_table[i];
    }
  }
  return NULL;
}

/**
 * @brief 验证魔数是否有效
 */
bool Upgrade_ValidateMagic(const UpgradeMagic_t *magic) {
  if (magic == NULL) {
    return false;
  }

  /* 验证前缀 */
  if (magic->prefix != UPGRADE_MAGIC_PREFIX) {
    return false;
  }

  /* 验证厂商代码不为0 */
  if (magic->vendor == CHIP_VENDOR_UNKNOWN) {
    return false;
  }

  /* 验证芯片型号不为0 */
  if (magic->chip == 0) {
    return false;
  }

  return true;
}

/**
 * @brief 验证魔数是否匹配当前芯片
 */
bool Upgrade_MatchCurrentChip(const UpgradeMagic_t *magic) {
  if (!Upgrade_ValidateMagic(magic)) {
    return false;
  }

  return (magic->vendor == CURRENT_CHIP_VENDOR &&
          magic->chip == CURRENT_CHIP_CODE);
}

/**
 * @brief 获取当前芯片信息
 */
const ChipInfo_t *Upgrade_GetCurrentChipInfo(void) { return &s_current_chip; }

/**
 * @brief 获取厂商名称字符串
 */
const char *Upgrade_GetVendorName(uint8_t vendor) {
  if (vendor < VENDOR_NAMES_SIZE && s_vendor_names[vendor] != NULL) {
    return s_vendor_names[vendor];
  }

  if (vendor == CHIP_VENDOR_CUSTOM) {
    return "Custom";
  }

  return "Unknown";
}

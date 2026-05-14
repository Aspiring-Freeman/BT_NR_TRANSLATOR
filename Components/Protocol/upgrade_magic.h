/**
 * @file upgrade_magic.h
 * @brief 通用固件升级协议魔数定义
 * @version 1.0.0
 * @date 2026-01-05
 *
 * @section intro 简介
 * 定义通用的固件升级协议魔数标识，用于区分不同芯片平台的升级命令。
 * 通过查表方式支持多种芯片，便于做成通用升级库。
 *
 * @section magic_format 魔数格式 (4字节)
 * ┌──────────────┬───────────────┬──────────────────────┐
 * │ Byte0: 标识  │ Byte1: 厂商   │ Byte2-3: 芯片型号    │
 * │ 0xF7 (固定)  │ 厂商代码      │ 型号代码 (小端)      │
 * └──────────────┴───────────────┴──────────────────────┘
 *
 * @section usage 使用方法
 * 1. 在升级命令帧数据区开头添加4字节魔数
 * 2. 接收端验证魔数后查表获取芯片信息
 * 3. 根据芯片信息配置Flash参数
 */

#ifndef __UPGRADE_MAGIC_H__
#define __UPGRADE_MAGIC_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 魔数固定标识
 *===========================================================================*/

/** @brief 升级协议魔数固定前缀 */
#define UPGRADE_MAGIC_PREFIX 0xF7

/*============================================================================
 * 芯片厂商代码定义 (Byte1)
 *===========================================================================*/

/** @brief 芯片厂商代码枚举 */
typedef enum {
  CHIP_VENDOR_UNKNOWN = 0x00, /**< 未知厂商 */
  CHIP_VENDOR_FMSH = 0x01,    /**< 复旦微电子 (FMSH) - FM33系列 */
  CHIP_VENDOR_GD = 0x02,      /**< 兆易创新 (GigaDevice) - GD32系列 */
  CHIP_VENDOR_ST = 0x03, /**< 意法半导体 (STMicroelectronics) - STM32系列 */
  CHIP_VENDOR_NUVOTON = 0x04,  /**< 新唐科技 (Nuvoton) - M0/M4系列 */
  CHIP_VENDOR_WCH = 0x05,      /**< 沁恒微电子 (WCH) - CH32系列 */
  CHIP_VENDOR_APM = 0x06,      /**< 极海半导体 (Geehy) - APM32系列 */
  CHIP_VENDOR_MM = 0x07,       /**< 灵动微电子 (MindMotion) - MM32系列 */
  CHIP_VENDOR_AT = 0x08,       /**< 雅特力科技 (Artery) - AT32系列 */
  CHIP_VENDOR_HOLYCHIP = 0x09, /**< 芯圣电子 (HolyChip) - HC32系列 */
  CHIP_VENDOR_HDSC = 0x0A,     /**< 华大半导体 (HDSC) - HC32系列 */
  CHIP_VENDOR_NATION = 0x0B,   /**< 国民技术 (Nations) - N32系列 */
  CHIP_VENDOR_ESPRESSIF = 0x0C, /**< 乐鑫 (Espressif) - ESP32系列 */
  CHIP_VENDOR_BK = 0x0D,        /**< 博通集成 (Beken) - BK72xx系列 */
  CHIP_VENDOR_REALTEK = 0x0E,   /**< 瑞昱 (Realtek) - RTL87xx系列 */
  /* 0x0F - 0xFE: 预留扩展 */
  CHIP_VENDOR_CUSTOM = 0xFF, /**< 自定义/私有厂商 */
} ChipVendorCode;

/*============================================================================
 * 芯片型号代码定义 (Byte2-3, 小端)
 *===========================================================================*/

/**
 * @brief 复旦微 (FMSH) 芯片型号代码
 * @note 编码规则: 0xSSMM (SS=系列, MM=型号)
 *       例如: FM33LG04x -> 0x3304 (33=FM33, 04=04x)
 */
typedef enum {
  /* FM33LG系列 - Cortex-M0+ */
  FMSH_FM33LG04X = 0x3304, /**< FM33LG04x: 256KB Flash, 24KB RAM */
  FMSH_FM33LG08X = 0x3308, /**< FM33LG08x: 512KB Flash, 48KB RAM */

  /* FM33LC系列 - Cortex-M0+ */
  FMSH_FM33LC04X = 0x3204, /**< FM33LC04x: 256KB Flash, 24KB RAM */

  /* FM33FT系列 */
  FMSH_FM33FT04X = 0x4604, /**< FM33FT04x */

  /* FM33FR系列 */
  FMSH_FM33FR04X = 0x4504, /**< FM33FR04x */
} FMSHChipCode;

/**
 * @brief 兆易创新 (GD) 芯片型号代码
 * @note 编码规则: 高字节=系列(F1=0x01,F3=0x03,E1=0xE1), 低字节=封装+Flash大小
 */
typedef enum {
  /* GD32F1系列 - Cortex-M3 */
  GD_GD32F103C8 = 0x0108, /**< GD32F103C8: 64KB Flash */
  GD_GD32F103RC = 0x01A6, /**< GD32F103RC: 256KB Flash (A=R封装, 6=256K) */

  /* GD32F3系列 - Cortex-M4 */
  GD_GD32F303CC = 0x03C6, /**< GD32F303CC: 256KB Flash */
  GD_GD32F303RC = 0x03A6, /**< GD32F303RC: 256KB Flash */

  /* GD32E系列 - Cortex-M33 */
  GD_GD32E103C8 = 0xE108, /**< GD32E103C8: 64KB Flash */
} GDChipCode;

/**
 * @brief 意法半导体 (ST) 芯片型号代码
 * @note 编码规则: 高字节=系列(F0=0x00,F1=0x01,F4=0x04,G0=0x10),
 * 低字节=封装+Flash大小
 */
typedef enum {
  /* STM32F0系列 - Cortex-M0 */
  ST_STM32F030C8 = 0x0008, /**< STM32F030C8: 64KB Flash */
  ST_STM32F030RC = 0x00A6, /**< STM32F030RC: 256KB Flash */

  /* STM32F1系列 - Cortex-M3 */
  ST_STM32F103C8 = 0x0108, /**< STM32F103C8: 64KB Flash */
  ST_STM32F103RC = 0x01A6, /**< STM32F103RC: 256KB Flash */

  /* STM32F4系列 - Cortex-M4 */
  ST_STM32F401CC = 0x04C6, /**< STM32F401CC: 256KB Flash */
  ST_STM32F407VE = 0x04D9, /**< STM32F407VE: 512KB Flash */

  /* STM32G0系列 - Cortex-M0+ */
  ST_STM32G030C8 = 0x1008, /**< STM32G030C8: 64KB Flash */
  ST_STM32G070RB = 0x10A5, /**< STM32G070RB: 128KB Flash */
} STChipCode;

/**
 * @brief 沁恒 (WCH) 芯片型号代码
 * @note 编码规则: 高字节=架构+系列(V1=0x01,V2=0x02,V3=0x03,F1=0x11),
 * 低字节=封装+Flash大小
 */
typedef enum {
  /* CH32V系列 - RISC-V */
  WCH_CH32V103C8 = 0x0108, /**< CH32V103C8: 64KB Flash */
  WCH_CH32V203C8 = 0x0208, /**< CH32V203C8: 64KB Flash */
  WCH_CH32V303RC = 0x03A6, /**< CH32V303RC: 256KB Flash */

  /* CH32F系列 - Cortex-M3 */
  WCH_CH32F103C8 = 0x1108, /**< CH32F103C8: 64KB Flash */
} WCHChipCode;

/*============================================================================
 * 芯片信息结构体
 *===========================================================================*/

/**
 * @brief 芯片信息结构体
 *
 * 用于存储芯片的详细信息，便于升级程序配置Flash参数
 */
typedef struct {
  uint8_t vendor_code;  /**< 厂商代码 */
  uint16_t chip_code;   /**< 芯片型号代码 */
  uint32_t flash_size;  /**< Flash 总大小 (字节) */
  uint32_t flash_start; /**< Flash 起始地址 */
  uint32_t page_size;   /**< Flash 页大小 (字节) */
  uint32_t sector_size; /**< Flash 扇区大小 (字节), 0表示无扇区概念 */
  uint32_t bootloader_size; /**< Bootloader 占用大小 (字节) */
  uint32_t app_start;       /**< APP 起始地址 */
  const char *name;         /**< 芯片名称字符串 */
} ChipInfo_t;

/*============================================================================
 * 魔数结构体
 *===========================================================================*/

#pragma pack(1)

/**
 * @brief 升级协议魔数结构体 (4字节)
 */
typedef struct {
  uint8_t prefix; /**< [0] 固定前缀: 0xF7 */
  uint8_t vendor; /**< [1] 厂商代码 */
  uint16_t chip;  /**< [2-3] 芯片型号代码 (小端) */
} UpgradeMagic_t;

#pragma pack()

/* 静态断言确保结构体大小 */
_Static_assert(sizeof(UpgradeMagic_t) == 4, "UpgradeMagic_t must be 4 bytes");

/*============================================================================
 * 当前项目芯片配置
 *===========================================================================*/

/** @brief 当前项目使用的芯片厂商 */
#define CURRENT_CHIP_VENDOR CHIP_VENDOR_FMSH

/** @brief 当前项目使用的芯片型号 */
#define CURRENT_CHIP_CODE FMSH_FM33LG04X

/** @brief 当前项目的魔数定义 */
#define CURRENT_UPGRADE_MAGIC                                                  \
  {                                                                            \
    UPGRADE_MAGIC_PREFIX, CURRENT_CHIP_VENDOR,                                 \
        (uint8_t)(CURRENT_CHIP_CODE & 0xFF),                                   \
        (uint8_t)((CURRENT_CHIP_CODE >> 8) & 0xFF)                             \
  }

/** @brief 当前项目的魔数 (32位整数, 小端) */
#define CURRENT_UPGRADE_MAGIC_U32                                              \
  ((uint32_t)UPGRADE_MAGIC_PREFIX | ((uint32_t)CURRENT_CHIP_VENDOR << 8) |     \
   ((uint32_t)CURRENT_CHIP_CODE << 16))

/*============================================================================
 * 芯片信息表 (可在C文件中实现)
 *===========================================================================*/

/**
 * @brief 根据魔数查找芯片信息
 * @param magic 4字节魔数
 * @return 芯片信息指针，未找到返回NULL
 */
const ChipInfo_t *Upgrade_FindChipInfo(const UpgradeMagic_t *magic);

/**
 * @brief 根据厂商和型号代码查找芯片信息
 * @param vendor 厂商代码
 * @param chip 芯片型号代码
 * @return 芯片信息指针，未找到返回NULL
 */
const ChipInfo_t *Upgrade_FindChipByCode(uint8_t vendor, uint16_t chip);

/**
 * @brief 验证魔数是否有效
 * @param magic 4字节魔数
 * @return true=有效, false=无效
 */
bool Upgrade_ValidateMagic(const UpgradeMagic_t *magic);

/**
 * @brief 验证魔数是否匹配当前芯片
 * @param magic 4字节魔数
 * @return true=匹配, false=不匹配
 */
bool Upgrade_MatchCurrentChip(const UpgradeMagic_t *magic);

/**
 * @brief 获取当前芯片信息
 * @return 当前芯片信息指针
 */
const ChipInfo_t *Upgrade_GetCurrentChipInfo(void);

/**
 * @brief 获取厂商名称字符串
 * @param vendor 厂商代码
 * @return 厂商名称字符串
 */
const char *Upgrade_GetVendorName(uint8_t vendor);

#ifdef __cplusplus
}
#endif

#endif /* __UPGRADE_MAGIC_H__ */
